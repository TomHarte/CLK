//
//  BBCMicro.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 14/09/2025.
//  Copyright © 2025 Thomas Harte. All rights reserved.
//

#include "BBCMicro.hpp"

#include "Machines/MachineTypes.hpp"

#include "Analyser/Static/Acorn/Target.hpp"

namespace BBCMicro {

class ConcreteMachine:
	public Machine,
	public MachineTypes::ScanProducer,
	public MachineTypes::TimedMachine
{
public:
	ConcreteMachine(
		const Analyser::Static::Acorn::BBCMicroTarget &target,
		const ROMMachine::ROMFetcher &rom_fetcher
	) {
		set_clock_rate(2'000'000);

		(void)target;
		(void)rom_fetcher;
	}

private:
	// MARK: - ScanProducer.
	void set_scan_target(Outputs::Display::ScanTarget *) override {}
	Outputs::Display::ScanStatus get_scan_status() const override {
		return Outputs::Display::ScanStatus{};
	}

	// MARK: - TimedMachine.
	void run_for(const Cycles) override {
	}
};

}

using namespace BBCMicro;

std::unique_ptr<Machine> Machine::BBCMicro(
	const Analyser::Static::Target *target,
	const ROMMachine::ROMFetcher &rom_fetcher
) {
	using Target = Analyser::Static::Acorn::BBCMicroTarget;
	const Target *const acorn_target = dynamic_cast<const Target *>(target);
	return std::make_unique<BBCMicro::ConcreteMachine>(*acorn_target, rom_fetcher);
}
