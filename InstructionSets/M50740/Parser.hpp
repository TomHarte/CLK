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

		while(start <= closing_bound) {
			const auto next = decoder.decode(&storage[start], 1 + closing_bound - start);
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
						case Operation::JMP: case Operation::BRA:
						return;

						default: break;
					}
				} else {
					// Check for end of stream and potential new entry points.
					switch(next.second.operation) {
						// Terminating instructions.
						case Operation::RTS: case Operation::RTI: case Operation::BRK:
						return;

						// Terminating operations, possibly with implied additional entry point.
						case Operation::JMP:
							if(next.second.addressing_mode == AddressingMode::Absolute) {
								target.add_entry(uint16_t(storage[start + 1] | (storage[start + 2] << 8)));
							}
						return;
						case Operation::BRA:
							target.add_entry(uint16_t(start + 2 + int8_t(storage[start + 1])));
						return;

						// Instructions that suggest another entry point but don't terminate parsing.
						case Operation::BCC: case Operation::BCS:
						case Operation::BVC: case Operation::BVS:
						case Operation::BMI: case Operation::BPL:
						case Operation::BNE: case Operation::BEQ:
							target.add_entry(uint16_t(start + 2 + int8_t(storage[start + 1])));
						break;
						case Operation::JSR:
							switch(next.second.addressing_mode) {
								default: break;
								case AddressingMode::Absolute:
									target.add_entry(uint16_t(storage[start + 1] | (storage[start + 2] << 8)));
								break;
								case AddressingMode::SpecialPage:
									target.add_entry(uint16_t(storage[start + 1] | 0x1f00));
								break;
							}
						break;

						case Operation::BBS0:	case Operation::BBS1:	case Operation::BBS2:	case Operation::BBS3:
						case Operation::BBS4:	case Operation::BBS5:	case Operation::BBS6:	case Operation::BBS7:
						case Operation::BBC0:	case Operation::BBC1:	case Operation::BBC2:	case Operation::BBC3:
						case Operation::BBC4:	case Operation::BBC5:	case Operation::BBC6:	case Operation::BBC7:
							switch(next.second.addressing_mode) {
								default: break;
								case AddressingMode::AccumulatorRelative:
									target.add_entry(uint16_t(start + 2 + int8_t(storage[start + 1])));
								break;
								case AddressingMode::ZeroPageRelative:
									target.add_entry(uint16_t(start + 3 + int8_t(storage[start + 2])));
								break;
							}
						break;


						default: break;
					}

					// Provide any fixed address accesses.
					switch(next.second.addressing_mode) {
						case AddressingMode::Absolute:
							target.add_access(uint16_t(storage[start + 1] | (storage[start + 2] << 8)), access_type(next.second.operation));
						break;
						case AddressingMode::ZeroPage:	case AddressingMode::ZeroPageRelative:
							target.add_access(storage[start + 1], access_type(next.second.operation));
						break;
						case AddressingMode::ImmediateZeroPage:
							target.add_access(storage[start + 2], access_type(next.second.operation));
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
