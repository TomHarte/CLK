//
//  Archimedes.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 04/03/2024.
//  Copyright © 2024 Thomas Harte. All rights reserved.
//

#include "Archimedes.hpp"

#include "../../AudioProducer.hpp"
#include "../../KeyboardMachine.hpp"
#include "../../MediaTarget.hpp"
#include "../../ScanProducer.hpp"
#include "../../TimedMachine.hpp"

#include "../../../InstructionSets/ARM/Executor.hpp"
#include "../../../Outputs/Log.hpp"

#include <algorithm>
#include <array>
#include <vector>

namespace {

Log::Logger<Log::Source::Archimedes> logger;

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
constexpr std::array<Zone, 0x20> zones(bool is_read) {
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

}

namespace Archimedes {

struct Video {
	void write(uint32_t value) {
		const auto target = (value >> 24) & 0xfc;

		switch(target) {
			case 0x00:	case 0x04:	case 0x08:	case 0x0c:
			case 0x10:	case 0x14:	case 0x18:	case 0x1c:
			case 0x20:	case 0x24:	case 0x28:	case 0x2c:
			case 0x30:	case 0x34:	case 0x38:	case 0x3c:
				logger.error().append("TODO: Video palette logical colour %d to %03x", (target >> 2), value & 0x1fff);
			break;

			case 0x40:
				logger.error().append("TODO: Video border colour to %03x", value & 0x1fff);
			break;

			case 0x44:	case 0x48:	case 0x4c:
				logger.error().append("TODO: Cursor colour %d to %03x", (target - 0x44) >> 2, value & 0x1fff);
			break;

			case 0x60:	case 0x64:	case 0x68:	case 0x6c:
			case 0x70:	case 0x74:	case 0x78:	case 0x7c:
				logger.error().append("TODO: Stereo image register %d to %03x", (target - 0x60) >> 2, value & 0x7);
			break;

			case 0x80:
				logger.error().append("TODO: Video horizontal period: %d", (value >> 14) & 0x3ff);
			break;
			case 0x84:
				logger.error().append("TODO: Video horizontal sync width: %d", (value >> 14) & 0x3ff);
			break;
			case 0x88:
				logger.error().append("TODO: Video horizontal border start: %d", (value >> 14) & 0x3ff);
			break;
			case 0x8c:
				logger.error().append("TODO: Video horizontal display start: %d", (value >> 14) & 0x3ff);
			break;
			case 0x90:
				logger.error().append("TODO: Video horizontal display end: %d", (value >> 14) & 0x3ff);
			break;
			case 0x94:
				logger.error().append("TODO: Video horizontal border end: %d", (value >> 14) & 0x3ff);
			break;
			case 0x98:
				logger.error().append("TODO: Video horizontal cursor end: %d", (value >> 14) & 0x3ff);
			break;
			case 0x9c:
				logger.error().append("TODO: Video horizontal interlace: %d", (value >> 14) & 0x3ff);
			break;

			case 0xa0:
				logger.error().append("TODO: Video vertical period: %d", (value >> 14) & 0x3ff);
			break;
			case 0xa4:
				logger.error().append("TODO: Video vertical sync width: %d", (value >> 14) & 0x3ff);
			break;
			case 0xa8:
				logger.error().append("TODO: Video vertical border start: %d", (value >> 14) & 0x3ff);
			break;
			case 0xac:
				logger.error().append("TODO: Video vertical display start: %d", (value >> 14) & 0x3ff);
			break;
			case 0xb0:
				logger.error().append("TODO: Video vertical display end: %d", (value >> 14) & 0x3ff);
			break;
			case 0xb4:
				logger.error().append("TODO: Video vertical border end: %d", (value >> 14) & 0x3ff);
			break;
			case 0xb8:
				logger.error().append("TODO: Video vertical cursor start: %d", (value >> 14) & 0x3ff);
			break;
			case 0xbc:
				logger.error().append("TODO: Video vertical cursor end: %d", (value >> 14) & 0x3ff);
			break;

			case 0xc0:
				logger.error().append("TODO: Sound frequency: %d", value & 0x7f);
			break;

			case 0xe0:
				logger.error().append("TODO: video control: %08x", value);
			break;

			default:
				logger.error().append("TODO: unrecognised VIDC write of %08x", value);
			break;
		}
	}
};

struct Interrupts {
	uint8_t irq_status_a() const {
		return irq_status_a_;
	}

	uint8_t irq_status_a_ = 0x80 | 0x10;	// 0x80 = always; 0x10 = at power-on.
};

/// Primarily models the MEMC.
struct Memory {
	void set_rom(const std::vector<uint8_t> &rom) {
		std::copy(
			rom.begin(),
			rom.begin() + static_cast<ptrdiff_t>(std::min(rom.size(), rom_.size())),
			rom_.begin());
	}

	template <typename IntT>
	bool write(uint32_t address, IntT source, InstructionSet::ARM::Mode mode, bool trans) {
		(void)trans;

		switch (write_zones_[(address >> 21) & 31]) {
			case Zone::DMAAndMEMC:
//				if(mode != InstructionSet::ARM::Mode::Supervisor) return false;
				if((address & 0b1110'0000'0000'0000'0000) == 0b1110'0000'0000'0000'0000) {
					// "The parameters are encoded into the processor address lines".
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

					logger.info().append("MEMC Control: %08x -> OS:%d sound:%d video:%d high:%d low:%d size:%d", address, os_mode_, sound_dma_enable_, video_dma_enable_, high_rom_access_time_, low_rom_access_time_, page_size_);
					update_mapping();

					return true;
				} else {
					logger.error().append("TODO: DMA/MEMC %08x to %08x", source, address);
				}
			break;

			case Zone::LogicallyMappedRAM: {
				const auto item = logical_ram<IntT, false>(address, mode);
				if(!item) {
					return false;
				}
				*item = source;
				return true;
			} break;

			case Zone::IOControllers:
				logger.error().append("TODO: Write to IO controllers of %08x to %08x", source, address);
			break;

			case Zone::VideoController:
				// TODO: handle byte writes correctly.
				vidc_.write(source);
			break;

			case Zone::PhysicallyMappedRAM:
//				if(mode != InstructionSet::ARM::Mode::Supervisor) return false;
				physical_ram<IntT>(address) = source;
			return true;

			case Zone::AddressTranslator:
				pages_[address & 0x7f] = address;
				update_mapping();
			break;

			default:
				printf("TODO: write of %08x to %08x [%lu]\n", source, address, sizeof(IntT));
			break;
		}

		return true;
	}

	template <typename IntT>
	bool read(uint32_t address, IntT &source, InstructionSet::ARM::Mode mode, bool trans) {
		(void)trans;
//		logger.info().append("R %08x", address);

		switch (read_zones_[(address >> 21) & 31]) {
			case Zone::PhysicallyMappedRAM:
//				if(mode != InstructionSet::ARM::Mode::Supervisor) return false;
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
				logger.error().append("TODO: Low ROM read from %08x", address);
			break;

			case Zone::HighROM:
				// Real test is: require A24=A25=0, then A25=1.
				// TODO: as above, move this test into the zones tables.
				has_moved_rom_ = true;
				source = high_rom<IntT>(address);
			return true;

			case Zone::IOControllers:
				switch(address & 0x7f) {
					default: break;

					case 0x10:	// IRQ status A
						source = ioc_.irq_status_a();
					return true;

					case 0x20:	// IRQ status B
						source = 0x00;
					return true;

					case 0x30:	// FIQ status
						source = 0x80;
					return true;
				}
				logger.error().append("TODO: IO controller read from %08x", address);
			break;

			default:
				logger.error().append("TODO: read from %08x", address);
			break;
		}

		source = 0;
		return true;
	}

	Memory() {
		// Install initial logical memory map.
		update_mapping();
	}

	private:
		bool has_moved_rom_ = false;
		std::array<uint8_t, 4*1024*1024> ram_{};
		std::array<uint8_t, 2*1024*1024> rom_;
		Interrupts ioc_;
		Video vidc_;

		template <typename IntT>
		IntT &physical_ram(uint32_t address) {
			return *reinterpret_cast<IntT *>(&ram_[address & (ram_.size() - 1)]);
		}

		template <typename IntT>
		IntT &high_rom(uint32_t address) {
			return *reinterpret_cast<IntT *>(&rom_[address & (rom_.size() - 1)]);
		}

		static constexpr std::array<Zone, 0x20> read_zones_ = zones(true);
		static constexpr std::array<Zone, 0x20> write_zones_ = zones(false);

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

		template <typename IntT, bool is_read>
		IntT *logical_ram(uint32_t address, InstructionSet::ARM::Mode mode) {
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
			logger.info().append("Updated logical RAM mapping");

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

			const auto bits = [](int start, int end) -> uint32_t {
				return ((1 << start) - 1) - ((1 << end) - 1);
			};

			// For each physical page, project it into logical space
			// and store it.
			for(const auto page: pages_) {
				uint32_t physical, logical;

				switch(size) {
					case PageSize::kb4:
						// 4kb:
						//		A[6:0] -> PPN[6:0]
						//		A[11:10] -> LPN[12:11];	A[22:12] -> LPN[10:0]	i.e. 8192 logical pages
						physical = page & bits(6, 0);

						physical <<= 12;

						logical = (page & bits(11, 10)) << 1;
						logical |= (page & bits(22, 12)) >> 12;
					break;

					case PageSize::kb8:
						// 8kb:
						//		A[0] -> PPN[6]; A[6:1] -> PPN[5:0]
						//		A[11:10] -> LPN[11:10];	A[22:13] -> LPN[9:0]	i.e. 4096 logical pages
						physical = (page & bits(0, 0)) << 6;
						physical |= (page & bits(6, 1)) >> 1;

						physical <<= 13;

						logical = page & bits(11, 10);
						logical |= (page & bits(22, 13)) >> 13;
					break;

					case PageSize::kb16:
						// 16kb:
						//		A[1:0] -> PPN[6:5]; A[6:2] -> PPN[4:0]
						//		A[11:10] -> LPN[10:9];	A[22:14] -> LPN[8:0]	i.e. 2048 logical pages
						physical = (page & bits(1, 0)) << 5;
						physical |= (page & bits(6, 2)) >> 2;

						physical <<= 14;

						logical = (page & bits(11, 10)) >> 1;
						logical |= (page & bits(22, 14)) >> 14;
					break;

					case PageSize::kb32:
						// 32kb:
						//		A[1] -> PPN[6]; A[2] -> PPN[5]; A[0] -> PPN[4]; A[6:3] -> PPN[6:3]
						//		A[11:10] -> LPN[9:8];	A[22:15] -> LPN[7:0]	i.e. 1024 logical pages
						physical = (page & bits(1, 1)) << 5;
						physical |= (page & bits(2, 2)) << 3;
						physical |= (page & bits(0, 0)) << 4;
						physical |= page & bits(6, 3);

						physical <<= 15;

						logical = (page & bits(11, 10)) >> 2;
						logical |= (page & bits(22, 15)) >> 15;
					break;
				}

				// TODO: consider clashes.
				// TODO: what if there's less than 4mb present?
				mapping_[logical].target = &ram_[physical];
				mapping_[logical].protection_level = (page >> 8) & 3;
			}
		}
};

class ConcreteMachine:
	public Machine,
	public MachineTypes::MediaTarget,
	public MachineTypes::TimedMachine,
	public MachineTypes::ScanProducer
{
	public:
		ConcreteMachine(
			const Analyser::Static::Target &target,
			const ROMMachine::ROMFetcher &rom_fetcher
		) {
			constexpr ROM::Name risc_os = ROM::Name::AcornRISCOS319;
			ROM::Request request(risc_os);
			auto roms = rom_fetcher(request);
			if(!request.validate(roms)) {
				throw ROMMachine::Error::MissingROMs;
			}

			executor_.bus.set_rom(roms.find(risc_os)->second);

			// TODO: pick a sensible clock rate; this is just code for '20 MIPS, please'.
			set_clock_rate(20'000'000);

			insert_media(target.media);
		}

	private:

		// MARK: - ScanProducer.
		void set_scan_target(Outputs::Display::ScanTarget *scan_target) override {
			(void)scan_target;
		}
		Outputs::Display::ScanStatus get_scaled_scan_status() const override {
			return Outputs::Display::ScanStatus();
		}

		// MARK: - TimedMachine.
		void run_for(Cycles cycles) override {
			static uint32_t last_pc = 0;

			auto instructions = cycles.as<int>();
			while(instructions--) {
				uint32_t instruction;
				if(!executor_.bus.read(executor_.pc(), instruction, executor_.registers().mode(), false)) {
					logger.info().append("Prefetch abort at %08x; last good was at %08x", executor_.pc(), last_pc);
					executor_.prefetch_abort();

					// TODO: does a double abort cause a reset?
					executor_.bus.read(executor_.pc(), instruction, executor_.registers().mode(), false);
				} else {
					last_pc = executor_.pc();
				}
				// TODO: pipeline prefetch?

//				static bool log = false;
//				if(log) {
//					logger.info().append("%08x: %08x", executor_.pc(), instruction);
//				}
				InstructionSet::ARM::execute<arm_model>(instruction, executor_);
			}
		}

		// MARK: - MediaTarget
		bool insert_media(const Analyser::Static::Media &) override {
//			int c = 0;
//			for(auto &disk : media.disks) {
//				fdc_.set_disk(disk, c);
//				c++;
//				if(c == 4) break;
//			}
//			return true;
			return false;
		}

		// MARK: - ARM execution
		static constexpr auto arm_model = InstructionSet::ARM::Model::ARMv2;
		InstructionSet::ARM::Executor<arm_model, Memory> executor_;
};

}

using namespace Archimedes;

std::unique_ptr<Machine> Machine::Archimedes(const Analyser::Static::Target *target, const ROMMachine::ROMFetcher &rom_fetcher) {
	return std::make_unique<ConcreteMachine>(*target, rom_fetcher);
}
