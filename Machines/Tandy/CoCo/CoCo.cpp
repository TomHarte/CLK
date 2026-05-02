//
//  CoCo.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 01/05/2026.
//  Copyright © 2026 Thomas Harte. All rights reserved.
//

#include "CoCo.hpp"

#include "Machines/MachineTypes.hpp"
#include "Analyser/Static/TandyCoCo/Target.hpp"

using namespace Tandy::CoCo;

namespace TandyCoCo {

class ConcreteMachine:
	public Machine,
	public MachineTypes::ScanProducer,
	public MachineTypes::TimedMachine
{
public:
	ConcreteMachine(const Analyser::Static::TandyCoCo::Target &target, const ROMMachine::ROMFetcher &rom_fetcher) {
		(void)target;
		(void)rom_fetcher;
	}

private:
	// MARK: - ScanProducer.

	void set_scan_target(Outputs::Display::ScanTarget *const target) final {
		(void)target;
	}

	Outputs::Display::ScanStatus get_scaled_scan_status() const final {
		return Outputs::Display::ScanStatus{};
	}

	// MARK: - TimedMachine.

	void run_for(const Cycles cycles) final {
		(void)cycles;
//		m6809_.run_for(cycles);
	}

	void flush_output(const int outputs) final {
		(void)outputs;
	}
};

}

std::unique_ptr<Machine> Machine::create(
	const Analyser::Static::Target &target,
	const ROMMachine::ROMFetcher &rom_fetcher
) {
	const auto &coco_target = static_cast<const Analyser::Static::TandyCoCo::Target &>(target);
	return std::make_unique<TandyCoCo::ConcreteMachine>(coco_target, rom_fetcher);
}
