//
//  Enterprise.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 10/06/2021.
//  Copyright © 2021 Thomas Harte. All rights reserved.
//

#include "Enterprise.hpp"

#include "../MachineTypes.hpp"

#include "../../Processors/Z80/Z80.hpp"

#include "../../Analyser/Static/Enterprise/Target.hpp"


namespace Enterprise {

class ConcreteMachine:
	public CPU::Z80::BusHandler,
	public Machine,
	public MachineTypes::ScanProducer,
	public MachineTypes::TimedMachine {
	public:
		ConcreteMachine(const Analyser::Static::Enterprise::Target &target, const ROMMachine::ROMFetcher &rom_fetcher) :
			z80(*this) {
			// Request a clock of 4Mhz; this'll be mapped upwards for Nick and Dave elsewhere.
			set_clock_rate(4'000'000);

			(void)target;
			(void)rom_fetcher;
		}

	private:
		CPU::Z80::Processor<ConcreteMachine, false, false> z80_;

		// MARK: - Z80::BusHandler.
		forceinline HalfCycles perform_machine_cycle(const CPU::Z80::PartialMachineCycle &cycle) {
			(void)cycle;
			return HalfCycles(0);
		}

		// MARK: - ScanProducer
		void set_scan_target(Outputs::Display::ScanTarget *scan_target) override {
			(void)scan_target;
		}

		Outputs::Display::ScanStatus get_scaled_scan_status() const override {
			return Outputs::Display::ScanStatus();
		}

		// MARK: - TimedMachine
		void run_for(const Cycles cycles) override {
			(void)cycles;
		}
};

}

using namespace Enterprise;

Machine *Machine::Enterprise(const Analyser::Static::Target *target, const ROMMachine::ROMFetcher &rom_fetcher) {
	using Target = Analyser::Static::Enterprise::Target;
	const Target *const enterprise_target = dynamic_cast<const Target *>(target);

	return new Enterprise::ConcreteMachine(*enterprise_target, rom_fetcher);
}

Machine::~Machine() {}
