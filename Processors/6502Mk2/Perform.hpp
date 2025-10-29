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
	registers.flags.template set_per<Flag::NegativeZero>(registers.a);
}

template <typename RegistersT>
void anc(RegistersT &registers, const uint8_t operand) {
	registers.a &= operand;
	registers.flags.template set_per<Flag::NegativeZero>(registers.a);
	registers.flags.template set_per<Flag::Carry>(registers.a >> 7);
}

template <Model model, typename RegistersT>
void adc(RegistersT &registers, const uint8_t operand) {
	uint8_t result = registers.a + operand + registers.flags.carry_value();
	uint8_t carry = result < registers.a + registers.flags.carry_value();

	if(!has_decimal_mode(model) || !registers.flags.template get<Flag::Decimal>()) {
		registers.flags.set_overflow(result, registers.a, operand);
		registers.flags.template set_per<Flag::NegativeZero>(registers.a = result);
		registers.flags.template set_per<Flag::Carry>(carry);
		return;
	}

	if constexpr (!is_65c02(model)) {
		registers.flags.template set_per<Flag::Zero>(result);
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
		carry |= result >= 0x100 - 0x6;
		result += 0x06;
	}

	// 6502 quirk: N and V are set before the full result is computed but
	// after the low nibble has been corrected.
	if constexpr (!is_65c02(model)) {
		registers.flags.template set_per<Flag::Negative>(result);
	}
	registers.flags.set_overflow(result, registers.a, operand);

	// i.e. fix high nibble if there was carry out of bit 7 already, or if the
	// top nibble is too large (in which case there will be carry after the fix-up).
	carry |= result >= 0xa0;
	if(carry) {
		result += 0x60;
	}

	registers.a = result;
	registers.flags.template set_per<Flag::Carry>(carry);
	if constexpr (is_65c02(model)) {
		registers.flags.template set_per<Flag::NegativeZero>(registers.a);
	}
}

template <Model model, typename RegistersT>
void sbc(RegistersT &registers, const uint8_t operand) {
	if(!has_decimal_mode(model) || !registers.flags.template get<Flag::Decimal>()) {
		adc<Model::NES6502>(registers, ~operand);	// Lie about the model to carry forward the fact of not-decimal.
		return;
	}

	const uint8_t operand_complement = ~operand;
	uint8_t result = registers.a + operand_complement + registers.flags.carry_value();

	// All flags are set based only on the decimal result.
	uint8_t carry = result < registers.a + registers.flags.carry_value();
	if constexpr (!is_65c02(model)) {
		registers.flags.template set_per<Flag::NegativeZero>(result);
	}
	registers.flags.set_overflow(result, registers.a, operand_complement);

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
	if(!carry) {
		result += 0xa0;
	}

	registers.a = result;
	registers.flags.template set_per<Flag::Carry>(carry);
	if constexpr (is_65c02(model)) {
		registers.flags.template set_per<Flag::NegativeZero>(registers.a);
	}
}

template <Model model, typename RegistersT>
void arr(RegistersT &registers, const uint8_t operand) {
	registers.a &= operand;
	const uint8_t unshifted_a = registers.a;
	registers.a = uint8_t((registers.a >> 1) | (registers.flags.carry_value() << 7));
	registers.flags.template set_per<Flag::NegativeZero>(registers.a);
	registers.flags.template set_per<Flag::Overflow>(uint8_t(registers.a ^ (registers.a << 1)));

	if(registers.flags.template get<Flag::Decimal>() && has_decimal_mode(model)) {
		if((unshifted_a&0xf) + (unshifted_a&0x1) > 5) registers.a = ((registers.a + 6)&0xf) | (registers.a & 0xf0);
		const uint8_t carry = ((unshifted_a&0xf0) + (unshifted_a&0x10) > 0x50) ? 1 : 0;
		if(carry) registers.a += 0x60;
		registers.flags.template set_per<Flag::Carry>(carry);
	} else {
		registers.flags.template set_per<Flag::Carry>((registers.a >> 6)&1);
	}
}

template <typename RegistersT>
void sbx(RegistersT &registers, const uint8_t operand) {
	registers.x &= registers.a;
	registers.flags.template set_per<Flag::Carry>(operand <= registers.x);
	registers.x -= operand;
	registers.flags.template set_per<Flag::NegativeZero>(registers.x);
}

template <typename RegistersT>
void asl(RegistersT &registers, uint8_t &operand) {
	registers.flags.template set_per<Flag::Carry>(operand >> 7);
	operand <<= 1;
	registers.flags.template set_per<Flag::NegativeZero>(operand);
}

template <typename RegistersT>
void aso(RegistersT &registers, uint8_t &operand) {
	registers.flags.template set_per<Flag::Carry>(operand >> 7);
	operand <<= 1;
	registers.a |= operand;
	registers.flags.template set_per<Flag::NegativeZero>(registers.a);
}

template <typename RegistersT>
void rol(RegistersT &registers, uint8_t &operand) {
	const uint8_t temp8 = uint8_t((operand << 1) | registers.flags.carry_value());
	registers.flags.template set_per<Flag::Carry>(operand >> 7);
	registers.flags.template set_per<Flag::NegativeZero>(operand = temp8);
}

template <typename RegistersT>
void rla(RegistersT &registers, uint8_t &operand) {
	const uint8_t temp8 = uint8_t((operand << 1) | registers.flags.carry_value());
	registers.flags.template set_per<Flag::Carry>(operand >> 7);
	operand = temp8;
	registers.a &= operand;
	registers.flags.template set_per<Flag::NegativeZero>(registers.a);
}

template <typename RegistersT>
void lsr(RegistersT &registers, uint8_t &operand) {
	registers.flags.template set_per<Flag::Carry>(operand & 1);
	operand >>= 1;
	registers.flags.template set_per<Flag::NegativeZero>(operand);
}

template <typename RegistersT>
void lse(RegistersT &registers, uint8_t &operand) {
	registers.flags.template set_per<Flag::Carry>(operand & 1);
	operand >>= 1;
	registers.a ^= operand;
	registers.flags.template set_per<Flag::NegativeZero>(registers.a);
}

template <typename RegistersT>
void asr(RegistersT &registers, uint8_t &operand) {
	registers.a &= operand;
	registers.flags.template set_per<Flag::Carry>(registers.a & 1);
	registers.a >>= 1;
	registers.flags.template set_per<Flag::NegativeZero>(registers.a);
}

template <typename RegistersT>
void ror(RegistersT &registers, uint8_t &operand) {
	const uint8_t temp8 = uint8_t((operand >> 1) | (registers.flags.carry_value() << 7));
	registers.flags.template set_per<Flag::Carry>(operand & 1);
	registers.flags.template set_per<Flag::NegativeZero>(operand = temp8);
}

template <Model model, typename RegistersT>
void rra(RegistersT &registers, uint8_t &operand) {
	const uint8_t temp8 = uint8_t((operand >> 1) | (registers.flags.carry_value() << 7));
	registers.flags.template set_per<Flag::Carry>(operand & 1);
	Operations::adc<model>(registers, temp8);
	operand = temp8;
}

template <typename RegistersT>
void compare(RegistersT &registers, const uint8_t lhs, const uint8_t rhs) {
	registers.flags.template set_per<Flag::Carry>(rhs <= lhs);
	registers.flags.template set_per<Flag::NegativeZero>(lhs - rhs);
}

template <typename RegistersT>
void bit(RegistersT &registers, const uint8_t operand) {
	registers.flags.template set_per<Flag::Zero>(operand & registers.a);
	registers.flags.template set_per<Flag::Negative>(operand);
	registers.flags.template set_per<Flag::Overflow>(operand);
}

template <typename RegistersT>
void bit_no_nv(RegistersT &registers, const uint8_t operand) {
	registers.flags.template set_per<Flag::Zero>(operand & registers.a);
}

template <typename RegistersT>
void trb(RegistersT &registers, uint8_t &operand) {
	registers.flags.template set_per<Flag::Zero>(operand & registers.a);
	operand &= ~registers.a;
}

template <typename RegistersT>
void tsb(RegistersT &registers, uint8_t &operand) {
	registers.flags.template set_per<Flag::Zero>(operand & registers.a);
	operand |= registers.a;
}

inline void rmb(uint8_t &operand, const uint8_t opcode) {
	operand &= ~(1 << (opcode >> 4));
}

inline void smb(uint8_t &operand, const uint8_t opcode) {
	operand |= 1 << ((opcode >> 4)&7);
}

template <typename RegistersT>
void sha(RegistersT &registers, RegisterPair16 &address, uint8_t &operand, const bool did_adjust_top) {
	if(did_adjust_top) {
		address.halves.high = operand = registers.a & registers.x & address.halves.high;
	} else {
		operand = registers.a & registers.x & (address.halves.high + 1);
	}
}

template <typename RegistersT>
void shx(RegistersT &registers, RegisterPair16 &address, uint8_t &operand, const bool did_adjust_top) {
	if(did_adjust_top) {
		address.halves.high = operand = registers.x & address.halves.high;
	} else {
		operand = registers.x & (address.halves.high + 1);
	}
}

template <typename RegistersT>
void shy(RegistersT &registers, RegisterPair16 &address, uint8_t &operand, const bool did_adjust_top) {
	if(did_adjust_top) {
		address.halves.high = operand = registers.y & address.halves.high;
	} else {
		operand = registers.y & (address.halves.high + 1);
	}
}

template <typename RegistersT>
void shs(RegistersT &registers, RegisterPair16 &address, uint8_t &operand, const bool did_adjust_top) {
	registers.s = registers.a & registers.x;
	if(did_adjust_top) {
		address.halves.high = operand = registers.s & address.halves.high;
	} else {
		operand = registers.s & (address.halves.high + 1);
	}
}

}

template <typename RegistersT>
bool test(const Operation operation, RegistersT &registers) {
	switch(operation) {
		default:
			__builtin_unreachable();

		case Operation::BPL: return !registers.flags.template get<Flag::Negative>();
		case Operation::BMI: return registers.flags.template get<Flag::Negative>();
		case Operation::BVC: return !registers.flags.template get<Flag::Overflow>();
		case Operation::BVS: return registers.flags.template get<Flag::Overflow>();
		case Operation::BCC: return !registers.flags.template get<Flag::Carry>();
		case Operation::BCS: return registers.flags.template get<Flag::Carry>();
		case Operation::BNE: return !registers.flags.template get<Flag::Zero>();
		case Operation::BEQ: return registers.flags.template get<Flag::Zero>();
		case Operation::BRA: return true;
	}
}

inline bool test_bbr_bbs(const uint8_t opcode, const uint8_t test_byte) {
	const auto mask = uint8_t(1 << ((opcode >> 4)&7));	// Get bit.
	const auto required = (opcode & 0x80) ? mask : 0;	// Check for BBR or BBS.
	return (test_byte & mask) == required;				// Compare.
}

template <Model model, typename RegistersT>
void perform(
	const Operation operation,
	RegistersT &registers,
	uint8_t &operand,
	const uint8_t opcode
) {
	switch(operation) {
		using enum Operation;

		default:
			__builtin_unreachable();

		case NOP:	break;

		// MARK: - Bitwise logic.

		case ORA:	registers.flags.template set_per<Flag::NegativeZero>(registers.a |= operand);	break;
		case AND:	registers.flags.template set_per<Flag::NegativeZero>(registers.a &= operand);	break;
		case EOR:	registers.flags.template set_per<Flag::NegativeZero>(registers.a ^= operand);	break;

		// MARK: - Loads and stores.

		case LDA:	registers.flags.template set_per<Flag::NegativeZero>(registers.a = operand);	break;
		case LDX:	registers.flags.template set_per<Flag::NegativeZero>(registers.x = operand);	break;
		case LDY:	registers.flags.template set_per<Flag::NegativeZero>(registers.y = operand);	break;
		case LAX:
			registers.flags.template set_per<Flag::NegativeZero>(registers.a = registers.x = operand);
		break;
		case LXA:
			registers.a = registers.x = (registers.a | 0xee) & operand;
			registers.flags.template set_per<Flag::NegativeZero>(registers.a);
		break;
		case PLP:	registers.flags = Flags(operand);								break;

		case STA:	operand = registers.a;											break;
		case STX:	operand = registers.x;											break;
		case STY:	operand = registers.y;											break;
		case STZ:	operand = 0;													break;
		case SAX:	operand = registers.a & registers.x;							break;
		case PHP:	operand = static_cast<uint8_t>(registers.flags) | Flag::Break;	break;

		case CLC:	registers.flags.template set_per<Flag::Carry>(0);					break;
		case CLI:	registers.flags.template set_per<Flag::Interrupt>(0);				break;
		case CLV:	registers.flags.template set_per<Flag::Overflow>(0);				break;
		case CLD:	registers.flags.template set_per<Flag::Decimal>(0);					break;
		case SEC:	registers.flags.template set_per<Flag::Carry>(Flag::Carry);			break;
		case SEI:	registers.flags.template set_per<Flag::Interrupt>(Flag::Interrupt);	break;
		case SED:	registers.flags.template set_per<Flag::Decimal>(Flag::Decimal);		break;

		case ANE:	Operations::ane(registers, operand);	break;
		case ANC:	Operations::anc(registers, operand);	break;
		case LAS:
			registers.a = registers.x = registers.s = registers.s & operand;
			registers.flags.template set_per<Flag::NegativeZero>(registers.a);
		break;

		// MARK: - Transfers.

		case TXA:	registers.flags.template set_per<Flag::NegativeZero>(registers.a = registers.x);	break;
		case TYA:	registers.flags.template set_per<Flag::NegativeZero>(registers.a = registers.y);	break;
		case TXS:	registers.s = registers.x;															break;
		case TAY:	registers.flags.template set_per<Flag::NegativeZero>(registers.y = registers.a);	break;
		case TAX:	registers.flags.template set_per<Flag::NegativeZero>(registers.x = registers.a);	break;
		case TSX:	registers.flags.template set_per<Flag::NegativeZero>(registers.x = registers.s);	break;

		// MARK: - Increments and decrements.

		case INC:	registers.flags.template set_per<Flag::NegativeZero>(++operand);		break;
		case DEC:	registers.flags.template set_per<Flag::NegativeZero>(--operand);		break;
		case INA:	registers.flags.template set_per<Flag::NegativeZero>(++registers.a);	break;
		case DEA:	registers.flags.template set_per<Flag::NegativeZero>(--registers.a);	break;
		case INX:	registers.flags.template set_per<Flag::NegativeZero>(++registers.x);	break;
		case DEX:	registers.flags.template set_per<Flag::NegativeZero>(--registers.x);	break;
		case INY:	registers.flags.template set_per<Flag::NegativeZero>(++registers.y);	break;
		case DEY:	registers.flags.template set_per<Flag::NegativeZero>(--registers.y);	break;

		// MARK: - Shifts and rolls.

		case ASL:	Operations::asl(registers, operand);		break;
		case ASO:	Operations::aso(registers, operand);		break;
		case ROL:	Operations::rol(registers, operand);		break;
		case RLA: 	Operations::rla(registers, operand);		break;
		case LSR:	Operations::lsr(registers, operand);		break;
		case LSE:	Operations::lse(registers, operand);		break;
		case ASR:	Operations::asr(registers, operand);		break;
		case ROR:	Operations::ror(registers, operand);		break;
		case RRA:	Operations::rra<model>(registers, operand);	break;

		// MARK: - Bit logic.

		case BIT:		Operations::bit(registers, operand);			break;
		case BITNoNV:	Operations::bit_no_nv(registers, operand);		break;
		case TRB:		Operations::trb(registers, operand);			break;
		case TSB:		Operations::tsb(registers, operand);			break;
		case RMB:		Operations::rmb(operand, opcode);				break;
		case SMB:		Operations::smb(operand, opcode);				break;

		// MARK: - Compare

		case DCP:
			--operand;
			Operations::compare(registers, registers.a, operand);
		break;
		case CMP:	Operations::compare(registers, registers.a, operand);	break;
		case CPX:	Operations::compare(registers, registers.x, operand);	break;
		case CPY:	Operations::compare(registers, registers.y, operand);	break;

		// MARK: - Arithmetic.

		case INS:
			++operand;
			Operations::sbc<model>(registers, operand);
		break;
		case SBC:	Operations::sbc<model>(registers, operand);		break;
		case ADC:	Operations::adc<model>(registers, operand);		break;
		case ARR:	Operations::arr<model>(registers, operand);		break;
		case SBX: 	Operations::sbx(registers, operand);			break;
	}
}

}
