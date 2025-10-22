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

		// MARK: - Loads and stores.

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

		case Operation::CLC:	registers.flags.carry = 0;								break;
		case Operation::CLI:	registers.flags.inverse_interrupt = Flag::Interrupt;	break;
		case Operation::CLV:	registers.flags.overflow = 0;							break;
		case Operation::CLD:	registers.flags.decimal = 0;							break;
		case Operation::SEC:	registers.flags.carry = Flag::Carry;					break;
		case Operation::SEI:	registers.flags.inverse_interrupt = 0;					break;
		case Operation::SED:	registers.flags.decimal = Flag::Decimal;				break;

		case Operation::ANE:
			registers.a = (registers.a | 0xee) & operand & registers.x;
			registers.flags.set_nz(registers.a);
		break;

		case Operation::ANC:
			registers.a &= operand;
			registers.flags.set_nz(registers.a);
			registers.flags.carry = registers.a >> 7;
		break;

		case Operation::LAS:
			registers.a = registers.x = registers.s = registers.s & operand;
			registers.flags.set_nz(registers.a);
		break;

		// MARK: - Increments and decrements.

		case Operation::INC:	registers.flags.set_nz(++operand);		break;
		case Operation::DEC:	registers.flags.set_nz(--operand);		break;
		case Operation::INA:	registers.flags.set_nz(++registers.a);	break;
		case Operation::DEA:	registers.flags.set_nz(--registers.a);	break;
		case Operation::INX:	registers.flags.set_nz(++registers.x);	break;
		case Operation::DEX:	registers.flags.set_nz(--registers.x);	break;
		case Operation::INY:	registers.flags.set_nz(++registers.y);	break;
		case Operation::DEY:	registers.flags.set_nz(--registers.y);	break;

		// MARK: - Shifts and rolls.

		case Operation::ASL:
			registers.flags.carry = operand >> 7;
			operand <<= 1;
			registers.flags.set_nz(operand);
		break;

		case Operation::ASO:
			registers.flags.carry = operand >> 7;
			operand <<= 1;
			registers.a |= operand;
			registers.flags.set_nz(registers.a);
		break;

		case Operation::ROL: {
			const uint8_t temp8 = uint8_t((operand << 1) | registers.flags.carry);
			registers.flags.carry = operand >> 7;
			registers.flags.set_nz(operand = temp8);
		} break;

		case Operation::RLA: {
			const uint8_t temp8 = uint8_t((operand << 1) | registers.flags.carry);
			registers.flags.carry = operand >> 7;
			operand = temp8;
			registers.a &= operand;
			registers.flags.set_nz(registers.a);
		} break;

		case Operation::LSR:
			registers.flags.carry = operand & 1;
			operand >>= 1;
			registers.flags.set_nz(operand);
		break;

		case Operation::LSE:
			registers.flags.carry = operand & 1;
			operand >>= 1;
			registers.a ^= operand;
			registers.flags.set_nz(registers.a);
		break;

		case Operation::ASR:
			registers.a &= operand;
			registers.flags.carry = registers.a & 1;
			registers.a >>= 1;
			registers.flags.set_nz(registers.a);
		break;

		case Operation::ROR: {
			const uint8_t temp8 = uint8_t((operand >> 1) | (registers.flags.carry << 7));
			registers.flags.carry = operand & 1;
			registers.flags.set_nz(operand = temp8);
		} break;

		case Operation::RRA: {
			const uint8_t temp8 = uint8_t((operand >> 1) | (registers.flags.carry << 7));
			registers.flags.carry = operand & 1;
			operand = temp8;
		} break;
	}
}

}
