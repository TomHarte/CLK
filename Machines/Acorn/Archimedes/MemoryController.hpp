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

#include "../../../InstructionSets/ARM/Registers.hpp"
#include "../../../Outputs/Log.hpp"

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
template <typename InterruptObserverT>
struct MemoryController {
	MemoryController(InterruptObserverT &observer) :
		ioc_(observer) {}

	int interrupt_mask() const {
		return ioc_.interrupt_mask();
	}

	void set_rom(const std::vector<uint8_t> &rom) {
		std::copy(
			rom.begin(),
			rom.begin() + static_cast<ptrdiff_t>(std::min(rom.size(), rom_.size())),
			rom_.begin());
	}

	template <typename IntT>
	uint32_t aligned(uint32_t address) {
		if constexpr (std::is_same_v<IntT, uint32_t>) {
			return address & static_cast<uint32_t>(~3);
		}
		return address;
	}

	template <typename IntT>
	bool write(uint32_t address, IntT source, InstructionSet::ARM::Mode mode, bool) {
		// User mode may only _write_ to logically-mapped RAM (subject to further testing below).
		if(mode == InstructionSet::ARM::Mode::User && address >= 0x200'0000) {
			return false;
		}

		switch(write_zones_[(address >> 21) & 31]) {
			case Zone::DMAAndMEMC: {
				const auto buffer_address = [](uint32_t source) -> uint32_t {
					return (source & 0x1fffc0) << 2;
				};

				// The MEMC itself isn't on the data bus; all values below should be taken from `address`.
				switch((address >> 17) & 0b111) {
					case 0b000:	ioc_.video().set_frame_start(buffer_address(address));	return true;
					case 0b001:	ioc_.video().set_buffer_start(buffer_address(address));	return true;
					case 0b010:	ioc_.video().set_buffer_end(buffer_address(address));	return true;
					case 0b011: ioc_.video().set_cursor_start(buffer_address(address));	return true;

					case 0b100:	ioc_.sound().set_next_start(buffer_address(address));	return true;
					case 0b101:	ioc_.sound().set_next_end(buffer_address(address));		return true;
					case 0b110:	ioc_.sound().swap();									return true;

					case 0b111:
						os_mode_ = address & (1 << 12);
						sound_dma_enable_ = address & (1 << 11);
						video_dma_enable_ = address & (1 << 10);
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

						logger.info().append("MEMC Control: %08x -> OS:%d sound:%d video:%d refresh:%d high:%d low:%d size:%d", address, os_mode_, sound_dma_enable_, video_dma_enable_, dynamic_ram_refresh_, high_rom_access_time_, low_rom_access_time_, page_size_);
						map_dirty_ = true;
					return true;
				}
			} break;

			case Zone::LogicallyMappedRAM: {
				const auto item = logical_ram<IntT, false>(address, mode);
				if(!item) {
					return false;
				}
				*item = source;
				return true;
			} break;

			case Zone::IOControllers:
				// TODO: have I overrestricted the value type for the IOC area?
				ioc_.write(address, uint8_t(source));
			return true;

			case Zone::VideoController:
				// TODO: handle byte writes correctly.
				ioc_.video().write(source);
			break;

			case Zone::PhysicallyMappedRAM:
				physical_ram<IntT>(address) = source;
			return true;

			case Zone::AddressTranslator:
//				printf("Translator write at %08x; replaces %08x\n", address, pages_[address & 0x7f]);
				pages_[address & 0x7f] = address;
				map_dirty_ = true;
			break;

			default:
//				printf("TODO: write of %08x to %08x [%lu]\n", source, address, sizeof(IntT));
			break;
		}

		return true;
	}

	template <typename IntT>
	bool read(uint32_t address, IntT &source, InstructionSet::ARM::Mode mode, bool) {
		// User mode may only read logically-maped RAM and ROM.
		if(mode == InstructionSet::ARM::Mode::User && address >= 0x200'0000 && address < 0x380'0000) {
			return false;
		}

		switch (read_zones_[(address >> 21) & 31]) {
			case Zone::PhysicallyMappedRAM:
				source = physical_ram<IntT>(address);
			return true;

			case Zone::LogicallyMappedRAM: {
				if(!has_moved_rom_) {	// TODO: maintain this state in the zones table.
					source = high_rom<IntT>(address);
					return true;
				}

				const auto item = logical_ram<IntT, true>(address, mode);
				if(!item) {
					return false;
				}
				source = *item;
				return true;
			} break;

			case Zone::LowROM:
//				logger.error().append("TODO: Low ROM read from %08x", address);
				source = IntT(~0);
			return true;

			case Zone::HighROM:
				// Real test is: require A24=A25=0, then A25=1.
				// TODO: as above, move this test into the zones tables.
				has_moved_rom_ = true;
				source = high_rom<IntT>(address);
			return true;

			case Zone::IOControllers:	{
				if constexpr (std::is_same_v<IntT, uint8_t>) {
					ioc_.read(address, source);
					return true;
				} else {
					// TODO: generalise this adaptation of an 8-bit device to the 32-bit bus, which probably isn't right anyway.
					uint8_t value;
					ioc_.read(address, value);
					source = value;
					return true;
				}
			}

			default:
				logger.error().append("TODO: read from %08x", address);
			break;
		}

		source = 0;
		return false;
	}

	void tick_timers() {	ioc_.tick_timers();		}
	void tick_sound() {
		// TODO: does disabling sound DMA pause output, or leave it ticking and merely
		// stop allowing it to use the bus?
		ioc_.sound().tick();
	}
	void tick_video() {
		ioc_.video().tick();
	}

	private:
		Log::Logger<Log::Source::ARMIOC> logger;

		enum class Zone {
			LogicallyMappedRAM,
			PhysicallyMappedRAM,
			IOControllers,
			LowROM,
			HighROM,
			VideoController,
			DMAAndMEMC,
			AddressTranslator,
		};
		static std::array<Zone, 0x20> zones(bool is_read) {
			std::array<Zone, 0x20> zones{};
			for(size_t c = 0; c < zones.size(); c++) {
				const auto address = c << 21;
				if(address < 0x200'0000) {
					zones[c] = Zone::LogicallyMappedRAM;
				} else if(address < 0x300'0000) {
					zones[c] = Zone::PhysicallyMappedRAM;
				} else if(address < 0x340'0000) {
					zones[c] = Zone::IOControllers;
				} else if(address < 0x360'0000) {
					zones[c] = is_read ? Zone::LowROM : Zone::VideoController;
				} else if(address < 0x380'0000) {
					zones[c] = is_read ? Zone::LowROM : Zone::DMAAndMEMC;
				} else {
					zones[c] = is_read ? Zone::HighROM : Zone::AddressTranslator;
				}
			}
			return zones;
		}

		bool has_moved_rom_ = false;
		std::array<uint8_t, 4*1024*1024> ram_{};
		std::array<uint8_t, 2*1024*1024> rom_;
		InputOutputController<InterruptObserverT> ioc_;

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

		const std::array<Zone, 0x20> read_zones_ = zones(true);
		const std::array<Zone, 0x20> write_zones_ = zones(false);

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

		// Logical to physical mapping.
		struct MappedPage {
			uint8_t *target = nullptr;
			uint8_t protection_level = 0;
		};
		std::array<MappedPage, 8192> mapping_;
		bool map_dirty_ = true;

		template <typename IntT, bool is_read>
		IntT *logical_ram(uint32_t address, InstructionSet::ARM::Mode mode) {
			// Possibly TODO: this recompute-if-dirty flag is supposed to ameliorate for an expensive
			// mapping process. It can be eliminated when the process is improved.
			if(map_dirty_) {
				update_mapping();
				map_dirty_ = false;
			}

			address = aligned<IntT>(address);
			address &= 0x1ff'ffff;
			size_t page;

			// TODO: eliminate switch here.
			switch(page_size_) {
				default:
				case PageSize::kb4:
					page = address >> 12;
					address &= 0x0fff;
				break;
				case PageSize::kb8:
					page = address >> 13;
					address &= 0x1fff;
				break;
				case PageSize::kb16:
					page = address >> 14;
					address &= 0x3fff;
				break;
				case PageSize::kb32:
					page = address >> 15;
					address &= 0x7fff;
				break;
			}

			if(!mapping_[page].target) {
				return nullptr;
			}

			// TODO: eliminate switch here.
			// Top of my head idea: is_read, is_user and is_os_mode make three bits, so
			// keep a one-byte bitmap of permitted accesses rather than the raw protection
			// level?
			switch(mapping_[page].protection_level) {
				case 0b00:	break;
				case 0b01:
					if(!is_read && mode == InstructionSet::ARM::Mode::User) {
						return nullptr;
					}
				break;
				default:
					if(mode == InstructionSet::ARM::Mode::User) {
						return nullptr;
					}
					if(!is_read && !os_mode_) {
						return nullptr;
					}
				break;
			}

			return reinterpret_cast<IntT *>(mapping_[page].target + address);
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
			std::fill(mapping_.begin(), mapping_.end(), MappedPage{});

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
				mapping_[logical].target = &ram_[physical];
				mapping_[logical].protection_level = (page >> 8) & 3;
			}
		}
};

}
