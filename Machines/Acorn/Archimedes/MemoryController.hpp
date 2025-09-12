//
//  MemoryController.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 20/03/2024.
//  Copyright © 2024 Thomas Harte. All rights reserved.
//

#pragma once

#include "InputOutputController.hpp"
#include "Video.hpp"
#include "Sound.hpp"

#include "InstructionSets/ARM/Registers.hpp"
#include "Outputs/Log.hpp"
#include "Activity/Observer.hpp"

#include <algorithm>

namespace Archimedes {

/// Provides the mask with all bits set in the range [start, end], where start must  be >= end.
template <int start, int end> struct BitMask {
	static_assert(start >= end);
	static constexpr uint32_t value = ((1 << (start + 1)) - 1) - ((1 << end) - 1);
};
static_assert(BitMask<0, 0>::value == 1);
static_assert(BitMask<1, 1>::value == 2);
static_assert(BitMask<15, 15>::value == 32768);
static_assert(BitMask<15, 0>::value == 0xffff);
static_assert(BitMask<15, 14>::value == 49152);


/// Models the MEMC, making this the Archimedes bus. Owns various other chips on the bus as a result.
template <typename InterruptObserverT, typename ClockRateObserverT>
struct MemoryController {
	MemoryController(InterruptObserverT &observer, ClockRateObserverT &clock_rate_observer) :
		ioc_(observer, clock_rate_observer, ram_.data()) {
		read_zones_[0] = ReadZone::HighROM;	// Temporarily put high ROM at address 0.
											// TODO: could I just copy it in? Or, at least,
											// could I detect at ROM loading time whether I can?
	}

	int interrupt_mask() const {
		return ioc_.interrupt_mask();
	}

	void set_rom(const std::vector<uint8_t> &rom) {
		if(rom_.size() % rom.size() || rom.size() > rom_.size()) {
			// TODO: throw.
			return;
		}

		// Copy in as many times as it'll fit.
		std::size_t base = 0;
		while(base < rom_.size()) {
			std::ranges::copy(
				rom,
				rom_.begin() + base);
			base += rom.size();
		}
	}

	template <typename IntT>
	uint32_t aligned(uint32_t address) {
		if constexpr (std::is_same_v<IntT, uint32_t>) {
			return address & static_cast<uint32_t>(~3);
		}
		return address;
	}

	template <typename IntT>
	bool write(uint32_t address, IntT source, InstructionSet::ARM::Mode, bool trans) {
		switch(write_zones_[(address >> 21) & 31]) {
			case WriteZone::LogicallyMappedRAM: {
				const auto item = logical_ram<IntT, false>(address, trans);
				if(item < reinterpret_cast<IntT *>(ram_.data())) {
					return false;
				}
				*item = source;
			} break;

			case WriteZone::PhysicallyMappedRAM:
				if(trans) return false;
				physical_ram<IntT>(address) = source;
			break;

			case WriteZone::DMAAndMEMC: {
				if(trans) return false;

				const auto buffer_address = [](uint32_t source) -> uint32_t {
					return (source & 0x1'fffc) << 2;
				};

				// The MEMC itself isn't on the data bus; all values below should be taken from `address`.
				switch((address >> 17) & 0b111) {
					case 0b000:	ioc_.video().set_frame_start(buffer_address(address));	break;
					case 0b001:	ioc_.video().set_buffer_start(buffer_address(address));	break;
					case 0b010:	ioc_.video().set_buffer_end(buffer_address(address));	break;
					case 0b011: ioc_.video().set_cursor_start(buffer_address(address));	break;

					case 0b100:	ioc_.sound().set_next_start(buffer_address(address));	break;
					case 0b101:	ioc_.sound().set_next_end(buffer_address(address));		break;
					case 0b110:	ioc_.sound().swap();									break;

					case 0b111:
						os_mode_ = address & (1 << 12);
						sound_dma_enable_ = address & (1 << 11);
						video_dma_enable_ = address & (1 << 10);
						ioc_.sound().set_dma_enabled(sound_dma_enable_);
						ioc_.video().set_dma_enabled(video_dma_enable_);
						switch((address >> 8) & 3) {
							default:
								dynamic_ram_refresh_ = DynamicRAMRefresh::None;
							break;
							case 0b01:
							case 0b11:
								dynamic_ram_refresh_ = DynamicRAMRefresh((address >> 8) & 3);
							break;
						}
						high_rom_access_time_ = ROMAccessTime((address >> 6) & 3);
						low_rom_access_time_ = ROMAccessTime((address >> 4) & 3);
						page_size_ = PageSize((address >> 2) & 3);
						switch(page_size_) {
							default:
							case PageSize::kb4:
								page_address_shift_ = 12;
								page_adddress_mask_ = 0x0fff;
							break;
							case PageSize::kb8:
								page_address_shift_ = 13;
								page_adddress_mask_ = 0x1fff;
							break;
							case PageSize::kb16:
								page_address_shift_ = 14;
								page_adddress_mask_ = 0x3fff;
							break;
							case PageSize::kb32:
								page_address_shift_ = 15;
								page_adddress_mask_ = 0x7fff;
							break;
						}

						Logger::info().append("MEMC Control: %08x -> OS:%d sound:%d video:%d refresh:%d high:%d low:%d size:%d", address, os_mode_, sound_dma_enable_, video_dma_enable_, dynamic_ram_refresh_, high_rom_access_time_, low_rom_access_time_, page_size_);
						map_dirty_ = true;
					break;
				}
			} break;

			case WriteZone::IOControllers:
				if(trans) return false;
				ioc_.template write<IntT>(address, source);
			break;

			case WriteZone::VideoController:
				if(trans) return false;
				// TODO: handle byte writes correctly.
				ioc_.video().write(source);
			break;

			case WriteZone::AddressTranslator:
				if(trans) return false;
//				printf("Translator write at %08x; replaces %08x\n", address, pages_[address & 0x7f]);
				pages_[address & 0x7f] = address;
				map_dirty_ = true;
			break;
		}

		return true;
	}

	template <typename IntT>
	bool read(uint32_t address, IntT &source, bool trans) {
		switch(read_zones_[(address >> 21) & 31]) {
			case ReadZone::LogicallyMappedRAM: {
				const auto item = logical_ram<IntT, true>(address, trans);
				if(item < reinterpret_cast<IntT *>(ram_.data())) {
					return false;
				}
				source = *item;
			} break;

			case ReadZone::HighROM:
				// Real test is: require A24=A25=0, then A25=1.
				read_zones_[0] = ReadZone::LogicallyMappedRAM;
				source = high_rom<IntT>(address);
			break;

			case ReadZone::PhysicallyMappedRAM:
				if(trans) return false;
				source = physical_ram<IntT>(address);
			break;

			case ReadZone::LowROM:
//				Logger::error().append("TODO: Low ROM read from %08x", address);
				source = IntT(~0);
			break;

			case ReadZone::IOControllers:
				if(trans) return false;
				ioc_.template read<IntT>(address, source);
			break;
		}

		return true;
	}

	template <typename IntT>
	bool read(uint32_t address, IntT &source, InstructionSet::ARM::Mode, bool trans) {
		return read(address, source, trans);
	}

	//
	// Expose various IOC-owned things.
	//
	void tick_timers()				{	ioc_.tick_timers();		}
	void tick_floppy(int clock_multiplier) {
		ioc_.tick_floppy(clock_multiplier);
	}
	void set_disk(std::shared_ptr<Storage::Disk::Disk> disk, size_t drive) {
		ioc_.set_disk(disk, drive);
	}

	Outputs::Speaker::Speaker *speaker() {
		return ioc_.sound().speaker();
	}

	auto &sound()					{	return ioc_.sound();	}
	const auto &sound() const		{	return ioc_.sound();	}
	auto &video()					{	return ioc_.video();	}
	const auto &video() const		{	return ioc_.video();	}
	auto &keyboard()				{	return ioc_.keyboard();	}
	const auto &keyboard() const	{	return ioc_.keyboard();	}

	void set_activity_observer(Activity::Observer *observer) {
		ioc_.set_activity_observer(observer);
	}

private:
	using Logger = Log::Logger<Log::Source::ARMIOC>;

	enum class ReadZone {
		LogicallyMappedRAM,
		PhysicallyMappedRAM,
		IOControllers,
		LowROM,
		HighROM,
	};
	enum class WriteZone {
		LogicallyMappedRAM,
		PhysicallyMappedRAM,
		IOControllers,
		VideoController,
		DMAAndMEMC,
		AddressTranslator,
	};
	template <bool is_read>
	using Zone = std::conditional_t<is_read, ReadZone, WriteZone>;

	template <bool is_read>
	static std::array<Zone<is_read>, 0x20> zones() {
		std::array<Zone<is_read>, 0x20> zones{};
		for(size_t c = 0; c < zones.size(); c++) {
			const auto address = c << 21;
			if(address < 0x200'0000) {
				zones[c] = Zone<is_read>::LogicallyMappedRAM;
			} else if(address < 0x300'0000) {
				zones[c] = Zone<is_read>::PhysicallyMappedRAM;
			} else if(address < 0x340'0000) {
				zones[c] = Zone<is_read>::IOControllers;
			} else if(address < 0x360'0000) {
				if constexpr (is_read) {
					zones[c] = Zone<is_read>::LowROM;
				} else {
					zones[c] = Zone<is_read>::VideoController;
				}
			} else if(address < 0x380'0000) {
				if constexpr (is_read) {
					zones[c] = Zone<is_read>::LowROM;
				} else {
					zones[c] = Zone<is_read>::DMAAndMEMC;
				}
			} else {
				if constexpr (is_read) {
					zones[c] = Zone<is_read>::HighROM;
				} else {
					zones[c] = Zone<is_read>::AddressTranslator;
				}
			}
		}
		return zones;
	}

	bool has_moved_rom_ = false;
	std::array<uint8_t, 2*1024*1024> rom_;
	std::array<uint8_t, 4*1024*1024> ram_{};
	InputOutputController<InterruptObserverT, ClockRateObserverT> ioc_;

	template <typename IntT>
	IntT &physical_ram(uint32_t address) {
		address = aligned<IntT>(address);
		address &= (ram_.size() - 1);
		return *reinterpret_cast<IntT *>(&ram_[address]);
	}

	template <typename IntT>
	IntT &high_rom(uint32_t address) {
		address = aligned<IntT>(address);
		return *reinterpret_cast<IntT *>(&rom_[address & (rom_.size() - 1)]);
	}

	std::array<ReadZone, 0x20> read_zones_ = zones<true>();
	const std::array<WriteZone, 0x20> write_zones_ = zones<false>();

	// Control register values.
	bool os_mode_ = false;
	bool sound_dma_enable_ = false;
	bool video_dma_enable_ = false;	// "Unaffected" by reset, so here picked arbitrarily.

	enum class DynamicRAMRefresh {
		None = 0b00,
		DuringFlyback = 0b01,
		Continuous = 0b11,
	} dynamic_ram_refresh_ = DynamicRAMRefresh::None;	// State at reset is undefined; constrain to a valid enum value.

	enum class ROMAccessTime {
		ns450 = 0b00,
		ns325 = 0b01,
		ns200 = 0b10,
		ns200with60nsNibble = 0b11,
	} high_rom_access_time_ = ROMAccessTime::ns450, low_rom_access_time_ = ROMAccessTime::ns450;

	enum class PageSize {
		kb4 = 0b00,
		kb8 = 0b01,
		kb16 = 0b10,
		kb32 = 0b11,
	} page_size_ = PageSize::kb4;
	int page_address_shift_ = 12;
	uint32_t page_adddress_mask_ = 0xffff;

	// Address translator.
	//
	// MEMC contains one entry per a physical page number, indicating where it goes logically.
	// Any logical access is tested against all 128 mappings. So that's backwards compared to
	// the ideal for an emulator, which would map from logical to physical, even if a lot more
	// compact — there are always 128 physical pages; there are up to 8192 logical pages.
	//
	// So captured here are both the physical -> logical map as representative of the real
	// hardware, and the reverse logical -> physical map, which is built (and rebuilt, and rebuilt)
	// from the other.

	// Physical to logical mapping.
	std::array<uint32_t, 128> pages_{};

	// Logical to physical mapping; this is divided by 'access mode'
	// (i.e. the combination of read/write, trans and OS mode flags,
	// as multipliexed by the @c mapping() function) because mapping
	// varies by mode — not just in terms of restricting access, but
	// actually presenting different memory.
	using MapTarget = std::array<uint8_t *, 8192>;
	std::array<MapTarget, 6> mapping_;

	template <bool is_read>
	MapTarget &mapping(bool trans, bool os_mode) {
		const size_t index = (is_read ? 1 : 0) | (os_mode ? 2 : 0) | ((trans && !os_mode) ? 4 : 0);
		return mapping_[index];
	}

	bool map_dirty_ = true;

	/// @returns A pointer to somewhere in @c ram_ if RAM is mapped to this area, or a pointer to somewhere lower than @c ram_.data() otherwise.
	template <typename IntT, bool is_read>
	IntT *logical_ram(uint32_t address, bool trans) {
		// Possibly TODO: this recompute-if-dirty flag is supposed to ameliorate for an expensive
		// mapping process. It can be eliminated when the process is improved.
		if(map_dirty_) {
			update_mapping();
			map_dirty_ = false;
		}
		address = aligned<IntT>(address);
		address &= 0x1ff'ffff;
		const size_t page = address >> page_address_shift_;

		const auto &map = mapping<is_read>(trans, os_mode_);
		address &= page_adddress_mask_;
		return reinterpret_cast<IntT *>(&map[page][address]);
	}

	void update_mapping() {
		// For each physical page, project it into logical space.
		switch(page_size_) {
			default:
			case PageSize::kb4:		update_mapping<PageSize::kb4>();	break;
			case PageSize::kb8:		update_mapping<PageSize::kb8>();	break;
			case PageSize::kb16:	update_mapping<PageSize::kb16>();	break;
			case PageSize::kb32:	update_mapping<PageSize::kb32>();	break;
		}
	}

	template <PageSize size>
	void update_mapping() {
		// Clear all logical mappings.
		for(auto &map: mapping_) {
			// Seed all pointers to an address sufficiently far lower than the beginning of RAM as to mark
			// the entire page as unmapped no matter what offset is added.
			std::fill(map.begin(), map.end(), ram_.data() - 32768);
		}

		// For each physical page, project it into logical space
		// and store it.
		for(const auto page: pages_) {
			uint32_t physical, logical;

			switch(size) {
				case PageSize::kb4:
					// 4kb:
					//		A[6:0] -> PPN[6:0]
					//		A[11:10] -> LPN[12:11];	A[22:12] -> LPN[10:0]	i.e. 8192 logical pages
					physical = page & BitMask<6, 0>::value;

					physical <<= 12;

					logical = (page & BitMask<11, 10>::value) << 1;
					logical |= (page & BitMask<22, 12>::value) >> 12;
				break;

				case PageSize::kb8:
					// 8kb:
					//		A[0] -> PPN[6]; A[6:1] -> PPN[5:0]
					//		A[11:10] -> LPN[11:10];	A[22:13] -> LPN[9:0]	i.e. 4096 logical pages
					physical = (page & BitMask<0, 0>::value) << 6;
					physical |= (page & BitMask<6, 1>::value) >> 1;

					physical <<= 13;

					logical = page & BitMask<11, 10>::value;
					logical |= (page & BitMask<22, 13>::value) >> 13;
				break;

				case PageSize::kb16:
					// 16kb:
					//		A[1:0] -> PPN[6:5]; A[6:2] -> PPN[4:0]
					//		A[11:10] -> LPN[10:9];	A[22:14] -> LPN[8:0]	i.e. 2048 logical pages
					physical = (page & BitMask<1, 0>::value) << 5;
					physical |= (page & BitMask<6, 2>::value) >> 2;

					physical <<= 14;

					logical = (page & BitMask<11, 10>::value) >> 1;
					logical |= (page & BitMask<22, 14>::value) >> 14;
				break;

				case PageSize::kb32:
					// 32kb:
					//		A[1] -> PPN[6]; A[2] -> PPN[5]; A[0] -> PPN[4]; A[6:3] -> PPN[3:0]
					//		A[11:10] -> LPN[9:8];	A[22:15] -> LPN[7:0]	i.e. 1024 logical pages
					physical = (page & BitMask<1, 1>::value) << 5;
					physical |= (page & BitMask<2, 2>::value) << 3;
					physical |= (page & BitMask<0, 0>::value) << 4;
					physical |= (page & BitMask<6, 3>::value) >> 3;

					physical <<= 15;

					logical = (page & BitMask<11, 10>::value) >> 2;
					logical |= (page & BitMask<22, 15>::value) >> 15;
				break;
			}

//				printf("%08x => physical %d -> logical %d\n", page, (physical >> 15), logical);

			// TODO: consider clashes.
			// TODO: what if there's less than 4mb present?
			const auto target = &ram_[physical];

			const auto set_supervisor = [&](bool read, bool write) {
				if(read) mapping<true>(false, false)[logical] = target;
				if(write) mapping<false>(false, false)[logical] = target;
			};

			const auto set_os = [&](bool read, bool write) {
				if(read) mapping<true>(true, true)[logical] = target;
				if(write) mapping<false>(true, true)[logical] = target;
			};

			const auto set_user = [&](bool read, bool write) {
				if(read) mapping<true>(true, false)[logical] = target;
				if(write) mapping<false>(true, false)[logical] = target;
			};

			set_supervisor(true, true);
			switch((page >> 8) & 3) {
				case 0b00:
					set_os(true, true);
					set_user(true, true);
				break;
				case 0b01:
					set_os(true, true);
					set_user(true, false);
				break;
				default:
					set_os(true, false);
					set_user(false, false);
				break;
			}
		}
	}
};

}
