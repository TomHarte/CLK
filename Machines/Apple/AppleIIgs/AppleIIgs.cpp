//
//  AppleIIgs.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 20/10/2020.
//  Copyright 2020 Thomas Harte. All rights reserved.
//

#include "AppleIIgs.hpp"

#include "../../MachineTypes.hpp"
#include "../../../Processors/65816/65816.hpp"

#include "../../../Analyser/Static/AppleIIgs/Target.hpp"

namespace Apple {
namespace IIgs {

class ConcreteMachine:
	public Apple::IIgs::Machine,
	public MachineTypes::TimedMachine,
	public MachineTypes::ScanProducer,
	public CPU::MOS6502Esque::BusHandler<uint32_t> {

	public:
		ConcreteMachine(const Analyser::Static::AppleIIgs::Target &target, const ROMMachine::ROMFetcher &rom_fetcher) :
			m65816_(*this) {

			set_clock_rate(14318180.0);

			using Target = Analyser::Static::AppleIIgs::Target;
			std::vector<ROMMachine::ROM> rom_descriptions;
			const std::string machine_name = "AppleIIgs";
			switch(target.model) {
				case Target::Model::ROM00:
					/* TODO */
				case Target::Model::ROM01:
					rom_descriptions.emplace_back(machine_name, "the Apple IIgs ROM01", "apple2gs.rom", 128*1024, 0x42f124b0);
				break;

				case Target::Model::ROM03:
					rom_descriptions.emplace_back(machine_name, "the Apple IIgs ROM03", "apple2gs.rom2", 256*1024, 0xde7ddf29);
				break;
			}
			const auto roms = rom_fetcher(rom_descriptions);
			if(!roms[0]) {
				throw ROMMachine::Error::MissingROMs;
			}
			rom_ = *roms[0];

			size_t ram_size = 0;
			switch(target.memory_model) {
				case Target::MemoryModel::TwoHundredAndFiftySixKB:
					ram_size = 256;
				break;

				case Target::MemoryModel::OneMB:
					ram_size = 256 + 1024;
				break;

				case Target::MemoryModel::EightMB:
					ram_size = 256 + 8 * 1024;
				break;
			}
			ram_.resize(ram_size * 1024);

			// TODO: establish initial bus mapping and storage.
		}

		void run_for(const Cycles cycles) override {
			m65816_.run_for(cycles);
		}

		void set_scan_target(Outputs::Display::ScanTarget *) override {
		}

		Outputs::Display::ScanStatus get_scaled_scan_status() const override {
			return Outputs::Display::ScanStatus();
		}

		forceinline Cycles perform_bus_operation(const CPU::WDC65816::BusOperation operation, const uint32_t address, uint8_t *const value) {
			const BankMapping &mapping = bank_mapping_[address >> 8];

			if(mapping.flags & BankMapping::IsIO) {
				// TODO: all IO accesses.
			} else {
				const BankStorage &storage = bank_storage_[mapping.destination];

				// TODO: branching below is predicated on the idea that an extra 64kb of scratch write area
				// and 64kb of 0xffs would be worse than branching due to the data set increase. Verify that?
				if(isReadOperation(operation)) {
					*value = storage.read ? storage.read[address & 0xffff] : 0xff;
				} else {
					if(storage.write) {
						storage.write[address & 0xffff] = *value;
						if(mapping.flags & BankMapping::IsShadowed) {
							bank_storage_[mapping.destination + 0xe0].write[address & 0xffff] = *value;
						}
					}
				}
			}

			Cycles duration;

			// Determine the cost of this access.
			if((mapping.flags & BankMapping::Is1Mhz) || ((mapping.flags & BankMapping::IsShadowed) && !isReadOperation(operation))) {
				// TODO: (i) get into phase; (ii) allow for the 1Mhz bus length being sporadically 16 rather than 14.
				duration = Cycles(14);
			} else {
				// TODO: (i) get into phase; (ii) allow for collisions with the refresh cycle.
				duration = Cycles(5);
			}
			fast_access_phase_ = (fast_access_phase_ + duration.as<int>()) % 5;		// TODO: modulo something else, to allow for refresh.
			slow_access_phase_ = (slow_access_phase_ + duration.as<int>()) % 14;	// TODO: modulo something else, to allow for stretched cycles.
			return duration;
		}

	private:
		CPU::WDC65816::Processor<ConcreteMachine, false> m65816_;

		int fast_access_phase_ = 0;
		int slow_access_phase_ = 0;

		// MARK: - Memory layout and storage.

		// Memory layout part 1: the bank mapping. Indexed by the top 16 bits of the address,
		// each entry provides the actual bank that should be used plus some flags affecting the
		// access: whether this section of memory is currently enabled for shadowing, whether
		// accesses should cost 1 Mhz, and whether this is actually an IO area.
		//
		// Implementation note: the shadow and IO flags are more sensibly part of this table;
		// logically the 1Mhz flag would ideally go with BankStorage but since there's no space
		// in there currently set aside for flags, keeping it in the mapping will do.
		struct BankMapping {
			uint8_t destination = 0;
			uint8_t flags = 0;

			enum Flag: uint8_t {
				IsShadowed = 1 << 0,
				Is1Mhz = 1 << 1,
				IsIO = 1 << 2,
			};
		};
		static_assert(sizeof(BankMapping) == 2);
		BankMapping bank_mapping_[65536];

		// Memory layout part 2: the bank storage. For each bank both a read and a write pointer
		// are offered, indicating where the contents of this bank actually reside.
		struct BankStorage {
			uint8_t *write = nullptr;
			const uint8_t *read = nullptr;
		};
		BankStorage bank_storage_[256];

		// Actual memory storage.
		std::vector<uint8_t> ram_;
		std::vector<uint8_t> rom_;
};

}
}

using namespace Apple::IIgs;

Machine *Machine::AppleIIgs(const Analyser::Static::Target *target, const ROMMachine::ROMFetcher &rom_fetcher) {
	return new ConcreteMachine(*dynamic_cast<const Analyser::Static::AppleIIgs::Target *>(target), rom_fetcher);
}

Machine::~Machine() {}
