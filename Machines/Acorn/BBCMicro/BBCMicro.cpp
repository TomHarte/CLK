//
//  BBCMicro.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 14/09/2025.
//  Copyright © 2025 Thomas Harte. All rights reserved.
//

#include "BBCMicro.hpp"

#include "Machines/MachineTypes.hpp"

#include "Processors/6502/6502.hpp"

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
	) :
		m6502_(*this)
	{
		set_clock_rate(2'000'000);

		(void)target;
		(void)rom_fetcher;
	}

	// MARK: - 6502 bus.
	Cycles perform_bus_operation(
		const CPU::MOS6502::BusOperation operation,
		const uint16_t address,
		uint8_t *const value
	) {
		(void)operation;
		(void)address;
		(void)value;

		return Cycles(1);
	}

private:
	// MARK: - ScanProducer.
	void set_scan_target(Outputs::Display::ScanTarget *) override {}
	Outputs::Display::ScanStatus get_scan_status() const override {
		return Outputs::Display::ScanStatus{};
	}

	// MARK: - TimedMachine.
	void run_for(const Cycles cycles) override {
		m6502_.run_for(cycles);
	}

	// MARK: - Components.
	CPU::MOS6502::Processor<CPU::MOS6502::Personality::P6502, ConcreteMachine, false> m6502_;
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
