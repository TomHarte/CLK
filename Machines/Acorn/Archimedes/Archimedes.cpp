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
		(void)mode;
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

					logger.info().append("MEMC Control: %08x/%08x -> OS:%d sound:%d video:%d high:%d low:%d size:%d", address, source, os_mode_, sound_dma_enable_, video_dma_enable_, high_rom_access_time_, low_rom_access_time_, page_size_);

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
				logger.error().append("TODO: Write to video controller of %08x to %08x", source, address);
			break;

			case Zone::PhysicallyMappedRAM:
//				if(mode != InstructionSet::ARM::Mode::Supervisor) return false;
				physical_ram<IntT>(address) = source;
			return true;

			case Zone::AddressTranslator:
				logger.error().append("TODO: Write address translator of %08x to %08x", source, address);
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
				has_moved_rom_ = true;
				source = high_rom<IntT>(address);
			return true;

			case Zone::IOControllers:
				switch(address & 0x7f) {
					default: break;

					case 0x10:	// IRQ status A
						source = 0x80;
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

	private:
		bool has_moved_rom_ = false;
		std::array<uint8_t, 4*1024*1024> ram_{};
		std::array<uint8_t, 2*1024*1024> rom_;

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
		uint32_t pages_[128]{};

		// Logical to physical mapping.
		struct MappedPage {
			uint8_t *target = nullptr;
			uint8_t protection_level = 0;
		};
		MappedPage mapping_[8192];

		template <typename IntT, bool is_read>
		IntT *logical_ram(uint32_t address, InstructionSet::ARM::Mode) {
			logger.error().append("TODO: Logical RAM mapping at %08x", address);
			return nullptr;
		}

		void update_mapping() {
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
			auto instructions = cycles.as<int>();
			while(instructions--) {
				uint32_t instruction;
				if(!executor_.bus.read(executor_.pc(), instruction, executor_.registers().mode(), false)) {
					executor_.prefetch_abort();

					// TODO: does a double abort cause a reset?
					executor_.bus.read(executor_.pc(), instruction, executor_.registers().mode(), false);
				}
				// TODO: pipeline prefetch?

				logger.info().append("%08x: %08x", executor_.pc(), instruction);
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
