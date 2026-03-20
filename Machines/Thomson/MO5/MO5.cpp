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
#include "Components/6821/6821.hpp"

using namespace Thomson::MO5;

// Video timing, as far as auto-translate lets me figure it out:
//
//	64 cycles/line;
//	56 lines post signalled vsync, then 200 of video, then 56 more, for 312 total.
//
// Start of vsync is connected to CPU IRQ.
//
// Within a line: ??? Who knows ???
//
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

		page_lower(false);
	}

	void run_for(const Cycles cycles) final {
		m6809_.run_for(cycles);
	}

	template <
		int address,
		CPU::M6809::ReadWrite read_write,
		typename ComponentT
	>
	static void access(ComponentT &component, CPU::M6809::data_t<read_write> value) {
		if constexpr (CPU::M6809::is_read(read_write)) {
			value = component.template read<address>();
		} else {
			component.template write<address>(value);
		}
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
		if constexpr (read_write == CPU::M6809::ReadWrite::NoData) {
			return Cycles(0);
		} else {
			if(address >= 0xa7c0 && address < 0xa800) {
				switch(address) {
					case 0xa7c0:	access<0xa7c0, read_write>(system_pia_, value);		break;
					case 0xa7c1:	access<0xa7c1, read_write>(system_pia_, value);		break;
					case 0xa7c2:	access<0xa7c2, read_write>(system_pia_, value);		break;
					case 0xa7c3:	access<0xa7c3, read_write>(system_pia_, value);		break;
				}
			} else {
				if constexpr (CPU::M6809::is_read(read_write)) {
					if(address < 0x2000) {
						value = start_pointer_[address];
					} else if(address >= 0xc000) {
						value = rom_[address - 0xc000];
					} else {
						value = ram_[address];
					}
				} else {
					if(address < 0x2000) {
						start_pointer_[address] = value;
					} else {
						ram_[address] = value;
					}
				}
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

	std::array<uint8_t, 0x10000 + 0x2000> ram_;
	std::array<uint8_t, 0x4000> rom_;
	uint8_t *start_pointer_ = nullptr;

	void page_lower(const bool attributes) {
		start_pointer_ = &ram_[attributes ? 0 : 0xf000];
	}

	Motorola::MC6821::MC6821<int> system_pia_;
		// TODO:
		//
		//	Port A:
		//		b0 = lower 8kb RAM paging;
		//		b1–4: border colour;
		//		b4: light pen button
		//		b6: tape output
		//		b7: tape input [and 0 = no tape; 1 = tape present]
		//
		//	Port B:
		//		b0 = 1-bit sound output;
		//		b1–3 = keyboard column;
		//		b4–6: keyboard line;
		//		b7: status of key at that position.
		//
		//	CA1: lightpen input (IRQA -> FIRQ)
		//	CA2: drive motor control
		//	CB1: 50Hz interrupt (IRQB -> IRQ)
		//	CB2: genlock enable, maybe?

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
