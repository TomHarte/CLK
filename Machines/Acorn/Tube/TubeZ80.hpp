//
//  TubeZ80.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 04/11/2025.
//  Copyright © 2025 Thomas Harte. All rights reserved.
//

#pragma once

#include "Processors/Z80/Z80.hpp"
#include "Machines/Utility/ROMCatalogue.hpp"

#include <algorithm>

namespace Acorn::Tube {

template <typename ULAT>
struct TubeZ80: public CPU::Z80::BusHandler {
public:
	static constexpr auto ROM = ROM::Name::BBCMicroZ80Tube122;
	void set_rom(std::vector<uint8_t> rom) {
		rom.resize(sizeof(rom_));
		std::copy(rom.begin(), rom.end(), std::begin(rom_));
	}

	TubeZ80(ULAT &ula) : z80_(*this), ula_(ula) {}

	void run_for(const Cycles cycles) {
		// Map from 2Mhz to 6Mhz.
		z80_.run_for(cycles * 3);
	}

	void set_irq() {	z80_.set_interrupt_line(true);	}
	void set_nmi() {	z80_.set_non_maskable_interrupt_line(true);	}
	void set_reset(const bool reset) {
		z80_.set_reset_line(reset);
		rom_visible_ |= reset;
	}

	HalfCycles perform_machine_cycle(const CPU::Z80::PartialMachineCycle &cycle) {
		if(!cycle.is_terminal()) {
			return HalfCycles(0);
		}

		const uint16_t address = *cycle.address;
		switch(cycle.operation) {
			case CPU::Z80::PartialMachineCycle::ReadOpcode:
				if(address == 0x66) {
					rom_visible_ = true;
				}
				rom_visible_ &= address < 0x8000;
				[[fallthrough]];
			case CPU::Z80::PartialMachineCycle::Read:
				if(rom_visible_ && address <= sizeof(rom_)) {
					*cycle.value = rom_[address];
					return HalfCycles(2);
				} else {
					*cycle.value = ram_[address];
				}
			break;

			case CPU::Z80::PartialMachineCycle::Write:
				ram_[address] = *cycle.value;
			break;

			case CPU::Z80::PartialMachineCycle::Interrupt:
				*cycle.value = 0xfe;
			break;

			case CPU::Z80::PartialMachineCycle::Input:
				*cycle.value = ula_.parasite_read(address);
				update_interrupts();
			break;

			case CPU::Z80::PartialMachineCycle::Output:
				ula_.parasite_write(address, *cycle.value);
			break;

			default: break;
		}

		return HalfCycles(0);
	}

private:
	void update_interrupts() {
		z80_.set_interrupt_line(ula_.has_parasite_irq());
		z80_.set_non_maskable_interrupt_line(ula_.has_parasite_nmi());
	}

	CPU::Z80::Processor<TubeZ80, false, false> z80_;
	bool rom_visible_ = true;

	uint8_t rom_[4096];
	uint8_t ram_[65536];
	ULAT &ula_;
};

}
