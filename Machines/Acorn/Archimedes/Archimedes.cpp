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
					logger.error().append("TODO: MEMC Control: %08x", source);
					break;
				} else {
					logger.error().append("TODO: DMA/MEMC %08x to %08x", source, address);
					break;
				}

			case Zone::PhysicallyMappedRAM:
//				if(mode != InstructionSet::ARM::Mode::Supervisor) return false;
				physical_ram<IntT>(address) = source;
			break;

			default:
				printf("TODO: write of %08x to %08x [%lu]\n", source, address, sizeof(IntT));
			break;
		}

		return true;
	}

	template <typename IntT>
	bool read(uint32_t address, IntT &source, InstructionSet::ARM::Mode mode, bool trans) {
		(void)mode;
		(void)trans;

		switch (read_zones_[(address >> 21) & 31]) {
			case Zone::PhysicallyMappedRAM:
//				if(mode != InstructionSet::ARM::Mode::Supervisor) return false;
				source = physical_ram<IntT>(address);
			return true;

			case Zone::LogicallyMappedRAM:
				if(!has_moved_rom_) {
					source = high_rom<IntT>(address);
					break;
				}
				logger.error().append("TODO: Logical RAM read from %08x", address);
			break;

			case Zone::HighROM:
				has_moved_rom_ = true;
				source = high_rom<IntT>(address);
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
				executor_.bus.read(executor_.pc(), instruction, executor_.registers().mode(), false);
				// TODO: what if abort? How about pipeline prefetch?

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
