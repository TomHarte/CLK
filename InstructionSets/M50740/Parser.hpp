//
//  Parser.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 16/01/21.
//  Copyright © 2021 Thomas Harte. All rights reserved.
//

#ifndef InstructionSets_M50740_Parser_hpp
#define InstructionSets_M50740_Parser_hpp

#include <cstdint>
#include "Decoder.hpp"
#include "../AccessType.hpp"

namespace InstructionSet {
namespace M50740 {

template<typename Target, bool include_entries_and_accesses> struct Parser {
	void parse(Target &target, const uint8_t *storage, uint16_t start, uint16_t closing_bound) {
		Decoder decoder;

		while(start != closing_bound) {
			const auto next = decoder.decode(&storage[start], closing_bound - start);
			if(next.first <= 0) {
				// If there weren't enough bytes left before the closing bound to complete
				// an instruction, but implicitly there were some bytes left, announce overflow
				// and terminate.
				target.announce_overflow(start);
				return;
			} else {
				// Pass on the instruction.
				target.announce_instruction(start, next.second);

				if constexpr(!include_entries_and_accesses) {
					// Do a simplified test: is this a terminating operation?
					switch(next.second.operation) {
						case Operation::RTS: case Operation::RTI: case Operation::BRK:
						case Operation::JMP:	case Operation::BRA:
						return;

						default: break;
					}
				} else {
					// Check for end of stream and potential new entry points.
					switch(next.second.operation) {
						// Terminating instructions.
						case Operation::RTS: case Operation::RTI: case Operation::BRK:
						return;

						// Terminating operations with implied additional entry point.
						case Operation::JMP:
							target.add_entry(storage[start + 1] | (storage[start + 2] << 8));
						return;
						case Operation::BRA:
							target.add_entry(start + 2 + int8_t(storage[start + 1]));
						return;

						// Instructions that suggest another entry point but don't terminate parsing.
						case Operation::BBS: case Operation::BBC:
						case Operation::BCC: case Operation::BCS:
						case Operation::BVC: case Operation::BVS:
						case Operation::BMI: case Operation::BPL:
						case Operation::BNE: case Operation::BEQ:
							target.add_entry(start + 2 + int8_t(storage[start + 1]));
						break;
						case Operation::JSR:
							target.add_entry(storage[start + 1] | (storage[start + 2] << 8));
						break;

						default: break;
					}

					// Provide any fixed address accesses.
					switch(next.second.addressing_mode) {
						case AddressingMode::Absolute:
							target.add_access(storage[start + 1] | (storage[start + 2] << 8), access_type(next.second.operation));
						break;
						case AddressingMode::ZeroPage:
						case AddressingMode::Bit0ZeroPage:	case AddressingMode::Bit1ZeroPage:
						case AddressingMode::Bit2ZeroPage:	case AddressingMode::Bit3ZeroPage:
						case AddressingMode::Bit4ZeroPage:	case AddressingMode::Bit5ZeroPage:
						case AddressingMode::Bit6ZeroPage:	case AddressingMode::Bit7ZeroPage:
							target.add_access(storage[start + 1], access_type(next.second.operation));
						break;
						case AddressingMode::SpecialPage:
							target.add_access(storage[start + 1] | 0x1f00, access_type(next.second.operation));
						break;
						case AddressingMode::ImmediateZeroPage:
							target.add_access(storage[start + 2], access_type(next.second.operation));
						break;
						case AddressingMode::Bit0AccumulatorRelative:
							target.add_access(start + 2 + int8_t(storage[start + 1]), access_type(next.second.operation));
						break;

						default: break;
					}
				}

				// Advance.
				start += next.first;
			}
		}
	}
};

}
}

#endif /* InstructionSets_M50740_Parser_hpp */
