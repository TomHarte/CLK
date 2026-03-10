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
	public MachineTypes::ScanProducer,
	public Machine
{
	ConcreteMachine(const Analyser::Static::Target &, const ROMMachine::ROMFetcher &rom_fetcher) :
		m6809_(*this)
	{
		set_clock_rate(1'000'000);

		const auto request = ROM::Request(ROM::Name::ThomasonMO5v11);
		auto roms = rom_fetcher(request);
		if(!request.validate(roms)) {
			throw ROMMachine::Error::MissingROMs;
		}

		const auto &rom = roms.find(ROM::Name::ThomasonMO5v11)->second;
		std::copy_n(rom.begin(), rom.size(), rom_.begin());
	}

	void run_for(const Cycles cycles) final {
		m6809_.run_for(cycles);
	}

	template <
		CPU::M6809::BusPhase bus_phase,
		CPU::M6809::ReadWrite read_write,
		CPU::M6809::BusState bus_state,
		typename AddressT
	>
	Cycles perform(
		[[maybe_unused]] const AddressT address,
		[[maybe_unused]] CPU::M6809::data_t<read_write> value
	) {
		printf("%s %04x\n", CPU::M6809::is_read(read_write) ? "Read from" : "Write to", +address);

		if constexpr (CPU::M6809::is_read(read_write)) {
			if(address >= 0xc000) {
				value = rom_[address - 0xc000];
				printf("Read %02x\n", rom_[address - 0xc000]);
			} else {
				value = 0xff;
				printf("UNIMPLEMENTED. Read 0xff\n");
			}
		}

		return Cycles(0);
	}

private:
	struct M6809Traits {
		static constexpr bool uses_mrdy = false;
		static constexpr auto pause_precision = CPU::M6809::PausePrecision::BetweenInstructions;
		using BusHandlerT = ConcreteMachine;
	};
	CPU::M6809::Processor<M6809Traits> m6809_;

	std::array<uint8_t, 0xf000 + 0x2000> ram_;
	std::array<uint8_t, 0x4000> rom_;

	// MARK: - ScanProducer.

	void set_scan_target(Outputs::Display::ScanTarget *) final {
	}

	Outputs::Display::ScanStatus get_scan_status() const final {
		return Outputs::Display::ScanStatus();
	}
};

}

std::unique_ptr<Machine> Machine::ThomsonMO(
	const Analyser::Static::Target *target,
	const ROMMachine::ROMFetcher &rom_fetcher
) {
	return std::make_unique<ConcreteMachine>(*target, rom_fetcher);
}
