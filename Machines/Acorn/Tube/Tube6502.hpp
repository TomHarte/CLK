//
//  Tube6502.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 03/11/2025.
//  Copyright © 2025 Thomas Harte. All rights reserved.
//

#pragma once

#include "TubeProcessor.hpp"

#include "Processors/6502Mk2/6502Mk2.hpp"
#include "Machines/Utility/ROMCatalogue.hpp"

#include <algorithm>

namespace Acorn::Tube {

template <typename ULAT>
class Processor<ULAT, TubeProcessor::WDC65C02> {
public:
	static constexpr auto ROM = ROM::Name::BBCMicro6502Tube110;
	void set_rom(std::vector<uint8_t> source) {
		source.resize(sizeof(rom_));
		std::copy(source.begin(), source.end(), rom_);
	}

	Processor(ULAT &ula) : m6502_(*this), ula_(ula) {}

	// By convention, these are cycles relative to the host's 2Mhz bus.
	// Multiply by 3/2 to turn that into the tube 6502's usual 3Mhz bus.
	void run_for(const Cycles cycles) {
		cycles_modulo_ += cycles * 3;
		m6502_.run_for(cycles_modulo_.divide(Cycles(2)));
	}

	template <CPU::MOS6502Mk2::BusOperation operation, typename AddressT>
	Cycles perform(const AddressT address, CPU::MOS6502Mk2::data_t<operation> value) {
		if(address >= 0xfef8 && address < 0xff00) {
			rom_visible_ = false;
			if constexpr (is_read(operation)) {
				value = ula_.parasite_read(address);
			} else {
				ula_.parasite_write(address, value);
			}
		} else {
			if constexpr (is_read(operation)) {
				constexpr uint16_t RomStart = sizeof(ram_) - sizeof(rom_);
				value = rom_visible_ && address >= RomStart ? rom_[address - RomStart] : ram_[address];
			} else {
				ram_[address] = value;
			}
		}
		return Cycles(1);
	}

	void set_irq(const bool active) {	m6502_.template set<CPU::MOS6502Mk2::Line::IRQ>(active);	}
	void set_nmi(const bool active) {	m6502_.template set<CPU::MOS6502Mk2::Line::NMI>(active);	}
	void set_reset(const bool reset) {
		m6502_.template set<CPU::MOS6502Mk2::Line::Reset>(reset);
		rom_visible_ |= reset;
	}

private:
	uint8_t rom_[2048];
	uint8_t ram_[65536];
	Cycles cycles_modulo_;

	struct M6502Traits {
		static constexpr auto uses_ready_line = false;
		static constexpr auto pause_precision = CPU::MOS6502Mk2::PausePrecision::AnyCycle;
		using BusHandlerT = Processor;
	};
	CPU::MOS6502Mk2::Processor<CPU::MOS6502Mk2::Model::WDC65C02, M6502Traits> m6502_;
	bool rom_visible_ = true;

	ULAT &ula_;
};

};
