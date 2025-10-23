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

namespace Operations {

template <typename RegistersT>
void ane(RegistersT &registers, const uint8_t operand) {
	registers.a = (registers.a | 0xee) & operand & registers.x;
	registers.flags.set_nz(registers.a);
}

template <typename RegistersT>
void anc(RegistersT &registers, const uint8_t operand) {
	registers.a &= operand;
	registers.flags.set_nz(registers.a);
	registers.flags.carry = registers.a >> 7;
}

template <Model model, typename RegistersT>
void adc(RegistersT &registers, const uint8_t operand) {
	if(!has_decimal_mode(model) || !registers.flags.decimal) {
		const uint8_t result = registers.a + operand + registers.flags.carry;
		registers.flags.carry = result < registers.a + registers.flags.carry;
		registers.flags.set_v(result, registers.a, operand);
		registers.flags.set_nz(registers.a = result);
		return;
	}

	uint8_t result = registers.a + operand + registers.flags.carry;
	registers.flags.carry = Numeric::carried_out<Numeric::Operation::Add, 7>(registers.a, operand, result);
	if constexpr (!is_65c02(model)) {
		registers.flags.zero_result = result;
	}

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
	if constexpr (!is_65c02(model)) {
		registers.flags.negative_result = result;
	}
	registers.flags.set_v(result, registers.a, operand);

	// i.e. fix high nibble if there was carry out of bit 7 already, or if the
	// top nibble is too large (in which case there will be carry after the fix-up).
	registers.flags.carry |= result >= 0xa0;
	if(registers.flags.carry) {
		result += 0x60;
	}

	registers.a = result;
	if constexpr (is_65c02(model)) {
		registers.flags.set_nz(registers.a);
	}
}

template <Model model, typename RegistersT>
void sbc(RegistersT &registers, const uint8_t operand) {
	if(!has_decimal_mode(model) || !registers.flags.decimal) {
		adc<Model::NES6502>(registers, ~operand);	// Lie about the model to carry forward the fact of not-decimal.
		return;
	}

	const uint8_t operand_complement = ~operand;
	uint8_t result = registers.a + operand_complement + registers.flags.carry;

	// All flags are set based only on the decimal result.
	if constexpr (!is_65c02(model)) {
		registers.flags.set_nz(result);
	}
	registers.flags.carry = Numeric::carried_out<Numeric::Operation::Add, 7>(registers.a, operand_complement, result);
	registers.flags.set_v(result, registers.a, operand_complement);

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
	if(!Numeric::carried_in<4>(registers.a, operand_complement, result)) {
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
	if constexpr (is_65c02(model)) {
		registers.flags.set_nz(registers.a);
	}
}

template <Model model, typename RegistersT>
void arr(RegistersT &registers, const uint8_t operand) {
	registers.a &= operand;
	const uint8_t unshifted_a = registers.a;
	registers.a = uint8_t((registers.a >> 1) | (registers.flags.carry << 7));
	registers.flags.set_nz(registers.a);
	registers.flags.overflow = (registers.a^(registers.a << 1))&Flag::Overflow;

	if(registers.flags.decimal && has_decimal_mode(model)) {
		if((unshifted_a&0xf) + (unshifted_a&0x1) > 5) registers.a = ((registers.a + 6)&0xf) | (registers.a & 0xf0);
		registers.flags.carry = ((unshifted_a&0xf0) + (unshifted_a&0x10) > 0x50) ? 1 : 0;
		if(registers.flags.carry) registers.a += 0x60;
	} else {
		registers.flags.carry = (registers.a >> 6)&1;
	}
}

template <typename RegistersT>
void sbx(RegistersT &registers, const uint8_t operand) {
	registers.x &= registers.a;
	registers.flags.carry = operand <= registers.x;
	registers.x -= operand;
	registers.flags.set_nz(registers.x);
}

template <typename RegistersT>
void asl(RegistersT &registers, uint8_t &operand) {
	registers.flags.carry = operand >> 7;
	operand <<= 1;
	registers.flags.set_nz(operand);
}

template <typename RegistersT>
void aso(RegistersT &registers, uint8_t &operand) {
	registers.flags.carry = operand >> 7;
	operand <<= 1;
	registers.a |= operand;
	registers.flags.set_nz(registers.a);
}

template <typename RegistersT>
void rol(RegistersT &registers, uint8_t &operand) {
	const uint8_t temp8 = uint8_t((operand << 1) | registers.flags.carry);
	registers.flags.carry = operand >> 7;
	registers.flags.set_nz(operand = temp8);
}

template <typename RegistersT>
void rla(RegistersT &registers, uint8_t &operand) {
	const uint8_t temp8 = uint8_t((operand << 1) | registers.flags.carry);
	registers.flags.carry = operand >> 7;
	operand = temp8;
	registers.a &= operand;
	registers.flags.set_nz(registers.a);
}

template <typename RegistersT>
void lsr(RegistersT &registers, uint8_t &operand) {
	registers.flags.carry = operand & 1;
	operand >>= 1;
	registers.flags.set_nz(operand);
}

template <typename RegistersT>
void lse(RegistersT &registers, uint8_t &operand) {
	registers.flags.carry = operand & 1;
	operand >>= 1;
	registers.a ^= operand;
	registers.flags.set_nz(registers.a);
}

template <typename RegistersT>
void asr(RegistersT &registers, uint8_t &operand) {
	registers.a &= operand;
	registers.flags.carry = registers.a & 1;
	registers.a >>= 1;
	registers.flags.set_nz(registers.a);
}

template <typename RegistersT>
void ror(RegistersT &registers, uint8_t &operand) {
	const uint8_t temp8 = uint8_t((operand >> 1) | (registers.flags.carry << 7));
	registers.flags.carry = operand & 1;
	registers.flags.set_nz(operand = temp8);
}

template <Model model, typename RegistersT>
void rra(RegistersT &registers, uint8_t &operand) {
	const uint8_t temp8 = uint8_t((operand >> 1) | (registers.flags.carry << 7));
	registers.flags.carry = operand & 1;
	Operations::adc<model>(registers, temp8);
	operand = temp8;
}

template <typename RegistersT>
void compare(RegistersT &registers, const uint8_t lhs, const uint8_t rhs) {
	registers.flags.carry = rhs <= lhs;
	registers.flags.set_nz(lhs - rhs);
}

}

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

		case Operation::ANE:	Operations::ane(registers, operand);	break;
		case Operation::ANC:	Operations::anc(registers, operand);	break;
		case Operation::LAS:
			registers.a = registers.x = registers.s = registers.s & operand;
			registers.flags.set_nz(registers.a);
		break;

		// MARK: - Transfers.

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

		case Operation::ASL:	Operations::asl(registers, operand);		break;
		case Operation::ASO:	Operations::aso(registers, operand);		break;
		case Operation::ROL:	Operations::rol(registers, operand);		break;
		case Operation::RLA: 	Operations::rla(registers, operand);		break;
		case Operation::LSR:	Operations::lsr(registers, operand);		break;
		case Operation::LSE:	Operations::lse(registers, operand);		break;
		case Operation::ASR:	Operations::asr(registers, operand);		break;
		case Operation::ROR:	Operations::ror(registers, operand);		break;
		case Operation::RRA:	Operations::rra<model>(registers, operand);	break;

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

		case Operation::DCP:
			--operand;
			Operations::compare(registers, registers.a, operand);
		break;
		case Operation::CMP:	Operations::compare(registers, registers.a, operand);	break;
		case Operation::CPX:	Operations::compare(registers, registers.x, operand);	break;
		case Operation::CPY:	Operations::compare(registers, registers.y, operand);	break;

		// MARK: - Arithmetic.

		case Operation::INS:
			++operand;
			Operations::sbc<model>(registers, operand);
		break;

		case Operation::SBC:	Operations::sbc<model>(registers, operand);		break;
		case Operation::ADC:	Operations::adc<model>(registers, operand);		break;
		case Operation::ARR:	Operations::arr<model>(registers, operand);		break;
		case Operation::SBX: 	Operations::sbx(registers, operand);			break;
	}
}

}
