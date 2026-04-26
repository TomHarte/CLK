//
//  ROM.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 26/04/2026.
//  Copyright © 2026 Thomas Harte. All rights reserved.
//

#pragma once

#include "Storage/Tape/Parsers/ThomsonMO.hpp"

namespace Thomson::FastLoader {

struct ROM: public Loader {
	static std::optional<uint16_t> detect(const uint16_t address, const MemoryAccess &) {
		// MO5: RDBITs is at 0xf168.
		// (and the MO6 doesn't make that ambiguous)
		if(address == 0xf16a) {
			return 0xf168;
		}

		// MO6: at 0xf3fd (and this isn't ambiguous by bank).
		if(address == 0xf3ff) {
			return 0xf3fd;
		}

		return std::nullopt;
	}

	std::pair<TrapAction, bool> did_trap(
		const uint16_t address,
		MemoryAccess &memory,
		CPU::M6809::Registers &registers,
		Storage::Tape::BinaryTapePlayer &player,
		Storage::Tape::TapeSerialiser &serialiser
	) override {
		// MO6: is this the correct ROM?
		if(memory.read(address) != 0xa6) {
			return std::make_pair(TrapAction::None, true);
		}

		//
		// Has caught RDBITS.
		//
		// Inputs:
		//
		//	M0044 = current tape polarity (complement if applicable).
		//	M0045 = byte in progress; ROL new bit into here.
		//
		// Additional output:
		//
		//	A = 00 or FF as per bit detected.
		//
		// TODO: MO6: check for 1200 or 2400 baud?
		Storage::Tape::Thomson::MO::Parser parser;
		const auto dp = registers.reg<CPU::M6809::R8::DP>();
		auto &polarity = memory[size_t((dp << 8) | 0x44)];
		auto &data = memory[size_t((dp << 8) | 0x45)];

		parser.seed_level(
			polarity & 0x80 ? Storage::Tape::Pulse::Low : Storage::Tape::Pulse::High
		);

		const auto offset = serialiser.offset();
		const auto bit = parser.bit(serialiser);
		if(!bit.has_value()) {
			serialiser.set_offset(offset);
			return std::make_pair(TrapAction::None, true);
		}

		data = uint8_t((data << 1) | uint8_t(*bit));
		if(!*bit) {
			polarity ^= 0xff;
		}
		registers.reg<CPU::M6809::R8::A>() = *bit ? 0xff : 0x00;

		// The parser reads up to the end of the bit. The ROM routine ends about two-thirds
		// of the way through the bit. So 'rewind' the tape a little.
		player.add_delay(Cycles(200));

		return std::make_pair(TrapAction::RTS, true);
	}
};
}
