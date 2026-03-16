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
		const AddressT address,
		CPU::M6809::data_t<read_write> value
	) {
		if constexpr (CPU::M6809::is_read(read_write)) {
//			printf("X: %04x\n", m6809_.registers().x);
			if(address >= 0xc000) {
				value = rom_[address - 0xc000];
				printf("%04x: ROM -> 0x%02x [S: %04x]\n", +address, rom_[address - 0xc000], m6809_.registers().s);
			} else {
				value = ram_[address];
				printf("%04x: RAM -> 0x%02x [S: %04x]\n", +address, ram_[address], m6809_.registers().s);
			}
		} else {
			ram_[address] = value;
			printf("%04x: RAM <- 0x%02x [S: %04x]\n", +address, value, m6809_.registers().s);
		}

		// TODO: the lower portion of memory can actually be paged, so the linear representation above isn't accurate.

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
