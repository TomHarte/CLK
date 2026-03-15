//
//  Perform.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 10/03/2026.
//  Copyright © 2026 Thomas Harte. All rights reserved.
//

#pragma once

#include "Registers.hpp"
#include "InstructionSets/6809/OperationMapper.hpp"

namespace CPU::M6809 {
namespace Implementation {

inline uint16_t get(Registers &registers, const int index) {
	const auto repeated = [](const uint8_t value) {
		return uint16_t((value << 8) | value);
	};

	switch(index) {
		case 0:	return registers.reg<R16::D>();
		case 1:	return registers.reg<R16::X>();
		case 2:	return registers.reg<R16::Y>();
		case 3:	return registers.reg<R16::U>();
		case 4:	return registers.reg<R16::S>();
		case 5:	return registers.reg<R16::PC>();

		case 8: return uint16_t(0xff00 | registers.reg<R8::A>());
		case 9: return uint16_t(0xff00 | registers.reg<R8::B>());
		case 10: return repeated(registers.reg<R8::CC>());
		case 11: return repeated(registers.reg<R8::DP>());

		default: return uint16_t(0xffff);
	}
}

inline void set(Registers &registers, const int index, const uint16_t value) {
	switch(index) {
		case 0:		registers.reg<R16::D>() = value;			break;
		case 1:		registers.reg<R16::X>() = value;			break;
		case 2:		registers.reg<R16::Y>() = value;			break;
		case 3:		registers.reg<R16::U>() = value;			break;
		case 4:		registers.reg<R16::S>() = value;			break;
		case 5:		registers.reg<R16::PC>() = value;			break;

		case 8:		registers.reg<R8::A>() = uint8_t(value);	break;
		case 9:		registers.reg<R8::B>() = uint8_t(value);	break;
		case 10:	registers.reg<R8::CC>() = uint8_t(value);	break;
		case 11:	registers.reg<R8::DP>() = uint8_t(value);	break;

		default: break;
	}
}
}

// MARK: - Arithmetic.

inline void inc(Registers &registers, uint8_t &value) {
	++value;
	registers.cc.set_nz(value);
	registers.cc.set<ConditionCode::Overflow>(value == 0x80);
}

template <R8 r>
void inc(Registers &registers) {
	inc(registers, registers.reg<r>());
}

inline void dec(Registers &registers, uint8_t &value) {
	--value;
	registers.cc.set_nz(value);
	registers.cc.set<ConditionCode::Overflow>(value == 0x7f);
}

template <R8 r>
void dec(Registers &registers) {
	dec(registers, registers.reg<r>());
}

inline void abx(Registers &registers) {
	registers.reg<R16::X>() += registers.reg<R8::B>();
}

inline void neg(Registers &registers, uint8_t &value) {
	registers.cc.set<ConditionCode::Overflow>(value == 0x80);
	registers.cc.set<ConditionCode::Carry>(!value);
	value = -value;
	registers.cc.set_nz(value);
}

template <R8 r>
void neg(Registers &registers) {
	neg(registers, registers.reg<r>());
}

inline void com(Registers &registers, uint8_t &value) {
	value = ~value;
	registers.cc.set_nz(value);
	registers.cc.set<ConditionCode::Overflow>(false);
	registers.cc.set<ConditionCode::Carry>(true);
}

template <R8 r>
void com(Registers &registers) {
	com(registers, registers.reg<r>());
}

template <R8 r, bool with_carry>
void add(Registers &registers, const uint8_t operand) {
	const uint8_t source = registers.reg<r>();
	const uint8_t result = source + operand + (with_carry ? registers.cc.carry() : 0);

	registers.cc.set_nz(result);
	registers.cc.set_overflow(result, source, operand);
	registers.cc.set<ConditionCode::Carry>(result < operand);

	const uint8_t half = (source & 0xf) + (operand & 0xf) + (with_carry ? registers.cc.carry() : 0);
	registers.cc.set<ConditionCode::HalfCarry>(half & 0x10);

	registers.reg<r>() = result;
}

inline void addd(Registers &registers, const uint16_t operand) {
	const uint16_t source = registers.reg<R16::D>();
	const uint16_t result = source + operand;

	registers.cc.set_nz(result);
	registers.cc.set<ConditionCode::Carry>(result < operand);
	registers.cc.set_overflow(result, source, operand);
}

template <R8 r, bool with_carry, bool store_result>
void sub(Registers &registers, const uint8_t operand) {
	const uint8_t source = registers.reg<r>();
	const uint8_t result = source - operand - (with_carry ? registers.cc.carry() : 0);

	registers.cc.set_nz(result);
	registers.cc.set_overflow(result, uint8_t(~source), operand);
	registers.cc.set<ConditionCode::Carry>(result > source);

	const uint8_t half = (source & 0xf) - (operand & 0xf) - (with_carry ? registers.cc.carry() : 0);
	registers.cc.set<ConditionCode::HalfCarry>(half & 0x10);

	if constexpr (store_result) registers.reg<r>() = result;
}

template <R16 r, bool store_result>
void sub(Registers &registers, const uint16_t operand) {
	const uint16_t source = registers.reg<r>();
	const uint16_t result = source - operand;

	registers.cc.set_nz(result);
	registers.cc.set_overflow(result, uint16_t(~source), operand);
	registers.cc.set<ConditionCode::Carry>(result > source);

	if constexpr (store_result) registers.reg<r>() = result;
}

inline void mul(Registers &registers) {
	const uint16_t result = registers.reg<R8::A>() * registers.reg<R8::B>();
	registers.reg<R16::D>() = result;
	registers.cc.set<ConditionCode::Zero>(result);
	registers.cc.set<ConditionCode::Carry>(result & 0x80);
}

inline void daa(Registers &registers) {
	uint8_t high = registers.reg<R8::A>() >> 4;
	uint8_t low = registers.reg<R8::A>() & 0x0f;

	if(
		registers.cc.get<ConditionCode::Carry>() ||
		high > 9 ||
		(high > 8 && low > 9)
	) {
		high += 6;
	}
	if(
		registers.cc.get<ConditionCode::HalfCarry>() ||
		low > 9
	) {
		low += 6;
	}

	const uint8_t result = uint8_t((high << 4) | (low & 0x0f));
	registers.cc.set_nz(result);
	registers.cc.set<ConditionCode::Carry>(high >= 16);
}

// MARK: - Logical.

template <R8 r>
void and_(Registers &registers, const uint8_t operand) {
	if constexpr (r == R8::CC) {
		registers.reg<R8::CC>() = registers.reg<R8::CC>() & operand;
	} else {
		registers.cc.set_nz(registers.reg<r>() &= operand);
		registers.cc.set<ConditionCode::Overflow>(false);
	}
}

template <R8 r>
void or_(Registers &registers, const uint8_t operand) {
	if constexpr (r == R8::CC) {
		registers.reg<R8::CC>() = registers.reg<R8::CC>() | operand;
	} else {
		registers.cc.set_nz(registers.reg<r>() |= operand);
		registers.cc.set<ConditionCode::Overflow>(false);
	}
}

template <R8 r>
void eor_(Registers &registers, const uint8_t operand) {
	registers.cc.set_nz(registers.reg<r>() ^= operand);
	registers.cc.set<ConditionCode::Overflow>(false);
}

template <R8 r>
void bit(Registers &registers, const uint8_t operand) {
	const uint8_t result = operand & registers.reg<r>();
	registers.cc.set_nz(result);
	registers.cc.set<ConditionCode::Overflow>(false);
}

inline void tst(Registers &registers, const uint8_t value) {
	registers.cc.set_nz(value);
	registers.cc.set<ConditionCode::Overflow>(false);
}

template <R8 r>
void tst(Registers &registers) {
	tst(registers, registers.reg<r>());
}

// MARK: - Shifts and rolls.

inline void lsl(Registers &registers, uint8_t &value) {
	// TODO: value for H?
	registers.cc.set<ConditionCode::Overflow>((value << 1) ^ value);
	registers.cc.set<ConditionCode::Carry>(value >> 7);
	value <<= 1;
	registers.cc.set_nz(value);
}

template <R8 r>
void lsl(Registers &registers) {
	lsl(registers, registers.reg<r>());
}

inline void asr(Registers &registers, uint8_t &value) {
	// TODO: value for H?
	registers.cc.set<ConditionCode::Carry>(value & 1);
	value = (value >> 1) | (value & 0x80);
	registers.cc.set_nz(value);
}

template <R8 r>
void asr(Registers &registers) {
	asr(registers, registers.reg<r>());
}

inline void lsr(Registers &registers, uint8_t &value) {
	registers.cc.set<ConditionCode::Carry>(value & 1);
	value >>= 1;
	registers.cc.set_nz(value);
}

template <R8 r>
void lsr(Registers &registers) {
	lsr(registers, registers.reg<r>());
}

inline void rol(Registers &registers, uint8_t &value) {
	registers.cc.set<ConditionCode::Overflow>((value ^ (value << 1)) & 0x80);
	const auto next_carry = value & 0x80;
	value = uint8_t((value << 1) | registers.cc.carry());
	registers.cc.set_nz(value);
	registers.cc.set<ConditionCode::Carry>(next_carry);
}

template <R8 r>
void rol(Registers &registers) {
	rol(registers, registers.reg<r>());
}

inline void ror(Registers &registers, uint8_t &value) {
	const auto next_carry = value & 1;
	value = uint8_t((value >> 1) | (registers.cc.carry() << 7));
	registers.cc.set_nz(value);
	registers.cc.set<ConditionCode::Carry>(next_carry);
}

template <R8 r>
void ror(Registers &registers) {
	ror(registers, registers.reg<r>());
}

// MARK: - Data Transfer.

template <R8 r>
void ld(Registers &registers, const uint8_t operand) {
	registers.reg<r>() = operand;
	registers.cc.set_nz(operand);
	registers.cc.set<ConditionCode::Overflow>(false);
}

template <R8 r>
void st(Registers &registers, uint8_t &operand) {
	operand = registers.reg<r>();
	registers.cc.set_nz(operand);
	registers.cc.set<ConditionCode::Overflow>(false);
}

template <R16 r>
void ld(Registers &registers, const uint16_t operand) {
	registers.reg<r>() = operand;
	registers.cc.set_nz(operand);
	registers.cc.set<ConditionCode::Overflow>(false);
}

template <R16 r>
void st(Registers &registers, uint16_t &operand) {
	operand = registers.reg<r>();
	registers.cc.set_nz(operand);
	registers.cc.set<ConditionCode::Overflow>(false);
}

inline void tfr(Registers &registers, const uint8_t operand) {
	const auto source = operand >> 4;
	const auto destination = operand & 0xf;

	const uint16_t source_value = Implementation::get(registers, source);
	Implementation::set(registers, destination, source_value);
}

inline void exg(Registers &registers, const uint8_t operand) {
	const auto top = operand >> 4;
	const auto bottom = operand & 0xf;

	const uint16_t source_values[] = {
		Implementation::get(registers, top),
		Implementation::get(registers, bottom)
	};
	Implementation::set(registers, bottom, source_values[0]);
	Implementation::set(registers, top, source_values[1]);
}

inline void clr(Registers &registers, uint8_t &value) {
	value = 0;

	registers.cc.set<ConditionCode::Carry>(false);
	registers.cc.set<ConditionCode::Overflow>(false);
	registers.cc.set<ConditionCode::Zero>(true);
	registers.cc.set<ConditionCode::Negative>(false);
}

template <R8 r>
void clr(Registers &registers) {
	clr(registers, registers.reg<r>());
}

inline void sex(Registers &registers) {
	const uint8_t bottom = registers.reg<R8::B>();
	registers.reg<R8::A>() = (bottom & 0x80) ? 0xff : 0x00;
	registers.cc.set_nz(bottom);
}

// MARK: - Control flow.

template <InstructionSet::M6809::Condition condition, std::unsigned_integral OperandT>
requires (sizeof(OperandT) <= 2)
void bra(Registers &registers, const OperandT operand) {
	if(!registers.cc.test<condition>()) {
		return;
	}

	if constexpr (sizeof(OperandT) == 2) {
		registers.reg<R16::PC>() += operand;
	} else {
		registers.reg<R16::PC>() += int8_t(operand);
	}
}

// MARK: - Dispatch.

/// Performs anything that doesn't involve calculating an effective address, manipulating the stack or affecting the ongoing flow of operation,
/// i.e. the subset of instructions that can be rationalised as not touching memory or having any knowledge of the instruction stream once
/// an appropriately-sized operand has been fetched.
inline void perform(const InstructionSet::M6809::Operation operation, Registers &registers, RegisterPair16 &operand) {
	auto &byte = operand.halves.low;
	auto &word = operand.full;

	using Condition = InstructionSet::M6809::Condition;
	switch(operation) {
		using enum InstructionSet::M6809::Operation;

		case None:
		case NOP: break;

		case ABX:	abx(registers);						break;

		case ADCA:	add<R8::A, true>(registers, byte);	break;
		case ADCB:	add<R8::B, true>(registers, byte);	break;
		case ADDA:	add<R8::A, false>(registers, byte);	break;
		case ADDB:	add<R8::B, false>(registers, byte);	break;
		case ADDD:	addd(registers, word);				break;

		case SBCA:	sub<R8::A, true, true>(registers, byte);	break;
		case SBCB:	sub<R8::B, true, true>(registers, byte);	break;
		case SUBA:	sub<R8::A, false, true>(registers, byte);	break;
		case SUBB:	sub<R8::B, false, true>(registers, byte);	break;
		case CMPA:	sub<R8::A, false, false>(registers, byte);	break;
		case CMPB:	sub<R8::B, false, false>(registers, byte);	break;

		case SUBD:	sub<R16::D, true>(registers, word);		break;
		case CMPX:	sub<R16::X, false>(registers, word);	break;
		case CMPD:	sub<R16::D, false>(registers, word);	break;
		case CMPY:	sub<R16::Y, false>(registers, word);	break;
		case CMPU:	sub<R16::U, false>(registers, word);	break;
		case CMPS:	sub<R16::S, false>(registers, word);	break;

		case DAA:	daa(registers);							break;

		case ANDA:	and_<R8::A>(registers, byte);			break;
		case ANDB:	and_<R8::B>(registers, byte);			break;
		case ANDCC:	and_<R8::CC>(registers, byte);			break;
		case ORA:	or_<R8::A>(registers, byte);			break;
		case ORB:	or_<R8::B>(registers, byte);			break;
		case ORCC:	or_<R8::CC>(registers, byte);			break;
		case EORA:	eor_<R8::A>(registers, byte);			break;
		case EORB:	eor_<R8::B>(registers, byte);			break;

		case INCA:	inc<R8::A>(registers);					break;
		case INCB:	inc<R8::B>(registers);					break;
		case INC:	inc(registers, byte);					break;
		case DECA:	dec<R8::A>(registers);					break;
		case DECB:	dec<R8::B>(registers);					break;
		case DEC:	dec(registers, byte);					break;

		case NEGA:	neg<R8::A>(registers);					break;
		case NEGB:	neg<R8::B>(registers);					break;
		case NEG:	neg(registers, byte);					break;
		case COMA:	com<R8::A>(registers);					break;
		case COMB:	com<R8::B>(registers);					break;
		case COM:	com(registers, byte);					break;

		case ASRA:	asr<R8::A>(registers);					break;
		case ASRB:	asr<R8::B>(registers);					break;
		case ASR:	asr(registers, byte);					break;
		case LSLA:	lsl<R8::A>(registers);					break;
		case LSLB:	lsl<R8::B>(registers);					break;
		case LSL:	lsl(registers, byte);					break;
		case LSRA:	lsr<R8::A>(registers);					break;
		case LSRB:	lsr<R8::B>(registers);					break;
		case LSR:	lsr(registers, byte);					break;
		case ROLA:	rol<R8::A>(registers);					break;
		case ROLB:	rol<R8::B>(registers);					break;
		case ROL:	rol(registers, byte);					break;
		case RORA:	ror<R8::A>(registers);					break;
		case RORB:	ror<R8::B>(registers);					break;
		case ROR:	ror(registers, byte);					break;

		case BCC:	bra<Condition::CC>(registers, byte);	break;
		case BCS:	bra<Condition::CS>(registers, byte);	break;
		case BEQ:	bra<Condition::EQ>(registers, byte);	break;
		case BGE:	bra<Condition::GE>(registers, byte);	break;
		case BGT:	bra<Condition::GT>(registers, byte);	break;
		case BHI:	bra<Condition::HI>(registers, byte);	break;
		case BLE:	bra<Condition::LE>(registers, byte);	break;
		case BLS:	bra<Condition::LS>(registers, byte);	break;
		case BLT:	bra<Condition::LT>(registers, byte);	break;
		case BMI:	bra<Condition::MI>(registers, byte);	break;
		case BNE:	bra<Condition::NE>(registers, byte);	break;
		case BPL:	bra<Condition::PL>(registers, byte);	break;
		case BRA:	bra<Condition::A>(registers, byte);		break;
		case BRN:	bra<Condition::N>(registers, byte);		break;
		case BVC:	bra<Condition::VC>(registers, byte);	break;
		case BVS:	bra<Condition::VS>(registers, byte);	break;

		case BITA:	bit<R8::A>(registers, byte);			break;
		case BITB:	bit<R8::B>(registers, byte);			break;

		case CLRA:	clr<R8::A>(registers);					break;
		case CLRB:	clr<R8::B>(registers);					break;
		case CLR:	clr(registers, byte);					break;

		case LBCC:	bra<Condition::CC>(registers, word);	break;
		case LBCS:	bra<Condition::CS>(registers, word);	break;
		case LBEQ:	bra<Condition::EQ>(registers, word);	break;
		case LBGE:	bra<Condition::GE>(registers, word);	break;
		case LBGT:	bra<Condition::GT>(registers, word);	break;
		case LBHI:	bra<Condition::HI>(registers, word);	break;
		case LBLE:	bra<Condition::LE>(registers, word);	break;
		case LBLS:	bra<Condition::LS>(registers, word);	break;
		case LBLT:	bra<Condition::LT>(registers, word);	break;
		case LBMI:	bra<Condition::MI>(registers, word);	break;
		case LBNE:	bra<Condition::NE>(registers, word);	break;
		case LBPL:	bra<Condition::PL>(registers, word);	break;
		case LBRA:	bra<Condition::A>(registers, word);		break;
		case LBRN:	bra<Condition::N>(registers, word);		break;
		case LBVC:	bra<Condition::VC>(registers, word);	break;
		case LBVS:	bra<Condition::VS>(registers, word);	break;

		case LDA:	ld<R8::A>(registers, byte);				break;
		case LDB:	ld<R8::B>(registers, byte);				break;
		case STA:	st<R8::A>(registers, byte);				break;
		case STB:	st<R8::B>(registers, byte);				break;

		case LDD:	ld<R16::D>(registers, word);			break;
		case LEAU:
		case LDU:	ld<R16::U>(registers, word);			break;
		case LEAX:
		case LDX:	ld<R16::X>(registers, word);			break;
		case LEAY:
		case LDY:	ld<R16::Y>(registers, word);			break;
		case LEAS:
		case LDS:	ld<R16::S>(registers, word);			break;
		case STD:	st<R16::D>(registers, word);			break;
		case STU:	st<R16::U>(registers, word);			break;
		case STX:	st<R16::X>(registers, word);			break;
		case STY:	st<R16::Y>(registers, word);			break;
		case STS:	st<R16::S>(registers, word);			break;

		case MUL:	mul(registers);							break;

		case TFR:	tfr(registers, byte);					break;
		case EXG:	exg(registers, byte);					break;

		case SEX:	sex(registers);							break;
		case TSTA:	tst<R8::A>(registers);					break;
		case TSTB:	tst<R8::B>(registers);					break;
		case TST:	tst(registers, byte);					break;

		case JMP:	ld<R16::PC>(registers, word);			break;

		// Flow control that requires stack access.
		case JSR:	case BSR:	case LBSR:
		case RTI:	case RTS:
		case SWI:	case SWI2:	case SWI3:
		case SYNC:	case RESET:	case CWAI:

		// Stack access.
		case PSHS:	case PULS:	case PSHU:	case PULU:

		// Operation selection.
		case Page1:	case Page2:
//		break;

		default: __builtin_unreachable();
	}
}

}
