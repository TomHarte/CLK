//
//  Enterprise.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 10/06/2021.
//  Copyright © 2021 Thomas Harte. All rights reserved.
//

#include "Enterprise.hpp"

#include "../MachineTypes.hpp"

#include "../../Analyser/Static/Enterprise/Target.hpp"

namespace Enterprise {

class ConcreteMachine:
	public Machine,
	public MachineTypes::ScanProducer,
	public MachineTypes::TimedMachine {
	public:
		ConcreteMachine(const Analyser::Static::Enterprise::Target &target, const ROMMachine::ROMFetcher &rom_fetcher) {
			// Request a clock of 4Mhz; this'll be mapped upwards for Nick and Dave elsewhere.
			set_clock_rate(4'000'000);

			(void)target;
			(void)rom_fetcher;
		}

	private:
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
