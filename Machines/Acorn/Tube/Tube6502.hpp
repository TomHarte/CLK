//
//  Tube6502.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 03/11/2025.
//  Copyright © 2025 Thomas Harte. All rights reserved.
//

#pragma once

#include "Processors/6502Mk2/6502Mk2.hpp"

#include <algorithm>

namespace Acorn::Tube {

struct Tube6502 {
public:
	Tube6502() : m6502_(*this) {}

	// By convention, these are cycles relative to the host's 2Mhz bus.
	void run_for(const Cycles cycles) {
		m6502_.run_for(cycles);	// TODO: multiply by 1.5. Or more.
	}

	template <CPU::MOS6502Mk2::BusOperation operation, typename AddressT>
	Cycles perform(const AddressT address, CPU::MOS6502Mk2::data_t<operation> value) {
		if(address >= 0xfef8 && address < 0xff00) {
//			printf("TODO: second processor FIFO access @ %04x\n", +address);
		}

		if constexpr (is_read(operation)) {
			value = ram_[address];
		} else {
			ram_[address] = value;
		}
		return Cycles(1);
	}

	void set_rom(const std::vector<uint8_t> &source) {
		// TODO: verify the ROM is 2kb.
		// TODO: determine whethe rthis really is ROM, or is ROM that should have copied itself to RAM, or is something else.
		std::copy(source.begin(), source.end(), &ram_[65536 - 2048]);
	}

private:
	uint8_t ram_[65536];

	struct M6502Traits {
		static constexpr auto uses_ready_line = false;
		static constexpr auto pause_precision = CPU::MOS6502Mk2::PausePrecision::AnyCycle;
		using BusHandlerT = Tube6502;
	};
	CPU::MOS6502Mk2::Processor<CPU::MOS6502Mk2::Model::M6502, M6502Traits> m6502_;
};

};
