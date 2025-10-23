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

#include "Numeric/Carry.hpp"

#pragma once

namespace CPU::MOS6502Mk2 {

template <typename RegistersT>
bool test(const Operation operation, RegistersT &registers) {
	switch(operation) {
		default:
			__builtin_unreachable();

		case Operation::BPL: return !(registers.flags.negative_result & 0x80);
		case Operation::BMI: return registers.flags.negative_result & 0x80;
		case Operation::BVC: return !registers.flags.overflow;
		case Operation::BVS: return registers.flags.overflow;
		case Operation::BCC: return !registers.flags.carry;
		case Operation::BCS: return registers.flags.carry;
		case Operation::BNE: return registers.flags.zero_result;
		case Operation::BEQ: return !registers.flags.zero_result;
		case Operation::BRA: return true;
	}
}

template <Model model, typename RegistersT>
void perform(
	const Operation operation,
	RegistersT &registers,
	uint8_t &operand,
	const uint8_t opcode
) {
	switch(operation) {
		default:
			__builtin_unreachable();

		case Operation::NOP:	break;

		// MARK: - Bitwise logic.

		case Operation::ORA:	registers.flags.set_nz(registers.a |= operand);		break;
		case Operation::AND:	registers.flags.set_nz(registers.a &= operand);		break;
		case Operation::EOR:	registers.flags.set_nz(registers.a ^= operand);		break;

		// MARK: - Loads and stores.

		case Operation::LDA:	registers.flags.set_nz(registers.a = operand);					break;
		case Operation::LDX:	registers.flags.set_nz(registers.x = operand);					break;
		case Operation::LDY:	registers.flags.set_nz(registers.y = operand);					break;
		case Operation::LAX:	registers.flags.set_nz(registers.a = registers.x = operand);	break;
		case Operation::LXA:
			registers.a = registers.x = (registers.a | 0xee) & operand;
			registers.flags.set_nz(registers.a);
		break;
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

		// MARK: - Tranfers.

		case Operation::TXA:	registers.flags.set_nz(registers.a = registers.x);	break;
		case Operation::TYA:	registers.flags.set_nz(registers.a = registers.y);	break;
		case Operation::TXS:	registers.s = registers.x;							break;
		case Operation::TAY:	registers.flags.set_nz(registers.y = registers.a);	break;
		case Operation::TAX:	registers.flags.set_nz(registers.x = registers.a);	break;
		case Operation::TSX:	registers.flags.set_nz(registers.x = registers.s);	break;

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

			// TODO: can this goto be eliminated?
			goto adc;
		} break;

		// MARK: - Bit logic.

		case Operation::BIT:
			registers.flags.zero_result = operand & registers.a;
			registers.flags.negative_result = operand;
			registers.flags.overflow = operand & Flag::Overflow;
		break;
		case Operation::BITNoNV:
			registers.flags.zero_result = operand & registers.a;
		break;
		case Operation::TRB:
			registers.flags.zero_result = operand & registers.a;
			operand &= ~registers.a;
		break;
		case Operation::TSB:
			registers.flags.zero_result = operand & registers.a;
			operand |= registers.a;
		break;
		case Operation::RMB:
			operand &= ~(1 << (opcode >> 4));
		break;
		case Operation::SMB:
			operand |= 1 << ((opcode >> 4)&7);
		break;

		// MARK: - Compare

		// TODO: probably don't need to use 16-bit arithmetic below.
		case Operation::DCP:
			--operand;
			[[fallthrough]];
		case Operation::CMP: {
			const uint16_t temp16 = registers.a - operand;
			registers.flags.set_nz(uint8_t(temp16));
			registers.flags.carry = ((~temp16) >> 8)&1;
		} break;

		case Operation::CPX: {
			const uint16_t temp16 = registers.x - operand;
			registers.flags.set_nz(uint8_t(temp16));
			registers.flags.carry = ((~temp16) >> 8)&1;
		} break;

		case Operation::CPY: {
			const uint16_t temp16 = registers.y - operand;
			registers.flags.set_nz(uint8_t(temp16));
			registers.flags.carry = ((~temp16) >> 8)&1;
		} break;

		// MARK: - Arithmetic.

		case Operation::INS:
			++operand;
			[[fallthrough]];
		case Operation::SBC:
			operand = ~operand;

			if(registers.flags.decimal && has_decimal_mode(model)) {
				uint8_t result = registers.a + operand + registers.flags.carry;

				// All flags are set based only on the decimal result.
				registers.flags.zero_result = result;
				registers.flags.carry = Numeric::carried_out<Numeric::Operation::Add, 7>(registers.a, operand, result);
				registers.flags.negative_result = result;
				registers.flags.overflow = (( (result ^ registers.a) & (result ^ operand) ) & 0x80) >> 1;

				// General SBC logic:
				//
				// Because the range of valid numbers starts at 0, any subtraction that should have
				// caused decimal carry and which requires a digit fix up will definitely have caused
				// binary carry: the subtraction will have crossed zero and gone into negative numbers.
				//
				// So just test for carry (well, actually borrow, which is !carry).

				// The bottom nibble is adjusted if there was borrow into the top nibble;
				// on a 6502 additional borrow isn't propagated but on a 65C02 it is.
				// This difference affects invalid BCD numbers only — valid numbers will
				// never be less than -9 so adding 10 will always generate carry.
				if(!Numeric::carried_in<4>(registers.a, operand, result)) {
					if constexpr (is_65c02(model)) {
						result += 0xfa;
					} else {
						result = (result & 0xf0) | ((result + 0xfa) & 0xf);
					}
				}

				// The top nibble is adjusted only if there was borrow out of the whole byte.
				if(!registers.flags.carry) {
					result += 0xa0;
				}

				registers.a = result;

				// fix up in case this was INS.
				if(operation == Operation::INS) operand = ~operand;

				if constexpr (is_65c02(model)) {
					// 65C02 fix: set the N and Z flags based on the final, decimal result.
					// Read into `operation_` for the sake of reading somewhere; the value isn't
					// used and INS will write `operand_` back to memory.
					registers.flags.set_nz(registers.a);
//					read_mem(operation_, address_.full);
					break;
				}
				break;
			}
			[[fallthrough]];

		case Operation::ADC:
			adc:
			if(registers.flags.decimal && has_decimal_mode(model)) {
				uint8_t result = registers.a + operand + registers.flags.carry;
				registers.flags.zero_result = result;
				registers.flags.carry = Numeric::carried_out<Numeric::Operation::Add, 7>(registers.a, operand, result);

				// General ADC logic:
				//
				// Detecting decimal carry means finding occasions when two digits added together totalled
				// more than 9. Within each four-bit window that means testing the digit itself and also
				// testing for carry — e.g. 5 + 5 = 0xA, which is detectable only by the value of the final
				// digit, but 9 + 9 = 0x18, which is detectable only by spotting the carry.

				// Only a single bit of carry can flow from the bottom nibble to the top.
				//
				// So if that carry already happened, fix up the bottom without permitting another;
				// otherwise permit the carry to happen (and check whether carry then rippled out of bit 7).
				if(Numeric::carried_in<4>(registers.a, operand, result)) {
					result = (result & 0xf0) | ((result + 0x06) & 0x0f);
				} else if((result & 0xf) > 0x9) {
					registers.flags.carry |= result >= 0x100 - 0x6;
					result += 0x06;
				}

				// 6502 quirk: N and V are set before the full result is computed but
				// after the low nibble has been corrected.
				registers.flags.negative_result = result;
				registers.flags.overflow = (( (result ^ registers.a) & (result ^ operand) ) & 0x80) >> 1;

				// i.e. fix high nibble if there was carry out of bit 7 already, or if the
				// top nibble is too large (in which case there will be carry after the fix-up).
				registers.flags.carry |= result >= 0xa0;
				if(registers.flags.carry) {
					result += 0x60;
				}

				registers.a = result;

				if constexpr (is_65c02(model)) {
					// 65C02 fix: N and Z are set correctly based on the final BCD result, at the cost of
					// an extra cycle.
					registers.flags.set_nz(registers.a);
//					read_mem(operand_, address_.full);
					break;
				}
			} else {
				const uint16_t result = uint16_t(registers.a) + uint16_t(operand) + uint16_t(registers.flags.carry);
				registers.flags.overflow = (( (result^registers.a)&(result^operand) )&0x80) >> 1;
				registers.flags.set_nz(registers.a = uint8_t(result));
				registers.flags.carry = (result >> 8)&1;
			}

			// fix up in case this was INS.
			if(operation == Operation::INS) operand = ~operand;
		break;

		case Operation::ARR:
			if(registers.flags.decimal && has_decimal_mode(model)) {
				registers.a &= operand;
				const uint8_t unshifted_a = registers.a;
				registers.a = uint8_t((registers.a >> 1) | (registers.flags.carry << 7));
				registers.flags.set_nz(registers.a);
				registers.flags.overflow = (registers.a^(registers.a << 1))&Flag::Overflow;

				if((unshifted_a&0xf) + (unshifted_a&0x1) > 5) registers.a = ((registers.a + 6)&0xf) | (registers.a & 0xf0);

				registers.flags.carry = ((unshifted_a&0xf0) + (unshifted_a&0x10) > 0x50) ? 1 : 0;
				if(registers.flags.carry) registers.a += 0x60;
			} else {
				registers.a &= operand;
				registers.a = uint8_t((registers.a >> 1) | (registers.flags.carry << 7));
				registers.flags.set_nz(registers.a);
				registers.flags.carry = (registers.a >> 6)&1;
				registers.flags.overflow = (registers.a^(registers.a << 1))&Flag::Overflow;
			}
		break;

		case Operation::SBX: {
			registers.x &= registers.a;
			const uint16_t difference = registers.x - operand;
			registers.x = uint8_t(difference);
			registers.flags.set_nz(registers.x);
			registers.flags.carry = ((difference >> 8)&1)^1;
		} break;

		// MARK: - Oddball address dependencies.

		case Operation::SHA:
//			if(address_.full != next_address_.full) {
//				address_.halves.high = operand_ = a_ & x_ & address_.halves.high;
//			} else {
//				operand_ = a_ & x_ & (address_.halves.high + 1);
//			}
		break;
		case Operation::SHY:
//			if(address_.full != next_address_.full) {
//				address_.halves.high = operand_ = y_ & address_.halves.high;
//			} else {
//				operand_ = y_ & (address_.halves.high + 1);
//			}
		break;
		case Operation::SHS:
//			if(address_.full != next_address_.full) {
//				s_ = a_ & x_;
//				address_.halves.high = operand_ = s_ & address_.halves.high;
//			} else {
//				s_ = a_ & x_;
//				operand_ = s_ & (address_.halves.high + 1);
//			}
		break;
	}
}

}
