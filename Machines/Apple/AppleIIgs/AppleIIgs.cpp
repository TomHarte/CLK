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
			return Cycles(5);
		}

	private:
		CPU::WDC65816::Processor<ConcreteMachine, false> m65816_;
};

}
}

using namespace Apple::IIgs;

Machine *Machine::AppleIIgs(const Analyser::Static::Target *target, const ROMMachine::ROMFetcher &rom_fetcher) {
	return new ConcreteMachine(*dynamic_cast<const Analyser::Static::AppleIIgs::Target *>(target), rom_fetcher);
}

Machine::~Machine() {}
