//
//  Perform.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 21/10/2025.
//  Copyright © 2025 Thomas Harte. All rights reserved.
//

#include "Decoder.hpp"
#include "Model.hpp"
#include "Registers.hpp"

#pragma once

namespace CPU::MOS6502Mk2 {

template <Model model, typename RegistersT>
void perform(const Operation operation, RegistersT &registers, uint8_t &operand, const uint8_t opcode) {
	(void)opcode;

	switch(operation) {
		default:
			__builtin_unreachable();

		case Operation::BRK:
		case Operation::NOP:
		case Operation::JAM:	break;

		case Operation::ORA:	registers.flags.set_nz(registers.a |= operand);		break;
		case Operation::AND:	registers.flags.set_nz(registers.a &= operand);		break;
		case Operation::EOR:	registers.flags.set_nz(registers.a ^= operand);		break;

		case Operation::LDA:	registers.flags.set_nz(registers.a = operand);					break;
		case Operation::LDX:	registers.flags.set_nz(registers.x = operand);					break;
		case Operation::LDY:	registers.flags.set_nz(registers.y = operand);					break;
		case Operation::LAX:	registers.flags.set_nz(registers.a = registers.x = operand);	break;
		case Operation::PLP:	registers.flags = Flags(operand);								break;

		case Operation::STA:	operand = registers.a;											break;
		case Operation::STX:	operand = registers.x;											break;
		case Operation::STY:	operand = registers.y;											break;
		case Operation::STZ:	operand = 0;													break;
		case Operation::SAX:	operand = registers.a & registers.x;							break;
		case Operation::PHP:	operand = static_cast<uint8_t>(registers.flags) | Flag::Break;	break;
	}
}

}
