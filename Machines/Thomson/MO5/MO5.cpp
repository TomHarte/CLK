//
//  MO5.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 04/03/2026.
//  Copyright © 2026 Thomas Harte. All rights reserved.
//

#include "MO5.hpp"

#include "Machines/MachineTypes.hpp"
#include "Processors/6809/6809.hpp"

using namespace Thomson::MO5;

namespace {

struct ConcreteMachine:
	public MachineTypes::TimedMachine,
	public Machine
{
	ConcreteMachine(const Analyser::Static::Target &, const ROMMachine::ROMFetcher &) :
		m6809_(*this)
	{}

	void run_for(const Cycles cycles) final {
		m6809_.run_for(cycles);
	}

	template <
		CPU::M6809::ReadWrite read_write,
		CPU::M6809::BusState bus_state,
		typename AddressT
	>
	Cycles perform(
		[[maybe_unused]] const AddressT address,
		[[maybe_unused]] CPU::M6809::data_t<read_write> value
	) {
		return Cycles(1);
	}

private:
	struct M6809Traits {
		static constexpr bool uses_mrdy = false;
		static constexpr auto pause_precision = CPU::M6809::PausePrecision::BetweenInstructions;
		using BusHandlerT = ConcreteMachine;
	};
	CPU::M6809::Processor<M6809Traits> m6809_;
};

}

std::unique_ptr<Machine> Machine::MO5(
	const Analyser::Static::Target *target,
	const ROMMachine::ROMFetcher &rom_fetcher
) {
	return std::make_unique<ConcreteMachine>(*target, rom_fetcher);
}

