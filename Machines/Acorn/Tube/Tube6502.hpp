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

template <typename ULAT>
struct Tube6502 {
public:
	Tube6502(ULAT &ula) : m6502_(*this), ula_(ula) {}

	// By convention, these are cycles relative to the host's 2Mhz bus.
	// Multiply by 3/2 to turn that into the tube 6502's usual 3Mhz bus.
	void run_for(const Cycles cycles) {
		cycles_modulo_ += cycles * 3;
		m6502_.run_for(cycles_modulo_.divide(Cycles(2)));
	}

	template <CPU::MOS6502Mk2::BusOperation operation, typename AddressT>
	Cycles perform(const AddressT address, CPU::MOS6502Mk2::data_t<operation> value) {
		if(address >= 0xfef8 && address < 0xff00) {
			if constexpr (is_read(operation)) {
				const uint8_t result = ula_.parasite_read(address);
				value = result;
				update_interrupts();
//				printf("Parasite read %04x of %02x\n", +address, result);
			} else {
				ula_.parasite_write(address, value);
//				printf("Parasite write %04x of %02x\n", +address, +value);
			}
		} else {
			if constexpr (is_read(operation)) {
				value = ram_[address];
			} else {
				ram_[address] = value;
			}
		}
		return Cycles(1);
	}

	void set_rom(const std::vector<uint8_t> &source) {
		// TODO: verify the ROM is 2kb.
		// TODO: determine whether this really is ROM, or is ROM that should have copied itself to RAM, or is something else.
		std::copy(source.begin(), source.end(), rom_);
		reinstall_rom();
	}

	void set_irq() {	m6502_.template set<CPU::MOS6502Mk2::Line::IRQ>(true);	}
	void set_nmi() {	m6502_.template set<CPU::MOS6502Mk2::Line::NMI>(true);	}
	void set_reset(const bool reset) {
		m6502_.template set<CPU::MOS6502Mk2::Line::Reset>(reset);
		if(reset) {
			reinstall_rom();
		}
	}

private:
	void update_interrupts() {
		m6502_.template set<CPU::MOS6502Mk2::Line::IRQ>(ula_.has_parasite_irq());
		m6502_.template set<CPU::MOS6502Mk2::Line::NMI>(ula_.has_parasite_nmi());
	}

	void reinstall_rom() {
		std::copy(std::begin(rom_), std::end(rom_), &ram_[65536 - 2048]);
	}

	uint8_t rom_[2048];
	uint8_t ram_[65536];
	Cycles cycles_modulo_;

	struct M6502Traits {
		static constexpr auto uses_ready_line = false;
		static constexpr auto pause_precision = CPU::MOS6502Mk2::PausePrecision::AnyCycle;
		using BusHandlerT = Tube6502;
	};
	CPU::MOS6502Mk2::Processor<CPU::MOS6502Mk2::Model::WDC65C02, M6502Traits> m6502_;

	ULAT &ula_;
};

};
