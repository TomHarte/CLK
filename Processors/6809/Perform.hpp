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
#include "ClockReceiver/ClockReceiver.hpp"

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
	registers.cc.set<ConditionCode::Carry>(value);
	value = uint8_t(-value);
	registers.cc.set_nz(value);
}

template <R8 r>
void neg(Registers &registers) {
	neg(registers, registers.reg<r>());
}

inline void com(Registers &registers, uint8_t &value) {
	value = uint8_t(~value);
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

	const uint8_t half = (source & 0xf) + (operand & 0xf) + (with_carry ? registers.cc.carry() : 0);
	registers.cc.set<ConditionCode::HalfCarry>(half & 0x10);

	registers.cc.set_nz(result);
	registers.cc.set_overflow(result, source, operand);
	registers.cc.set<ConditionCode::Carry>(result < operand || (with_carry && registers.cc.carry() && result <= operand));

	registers.reg<r>() = result;
}

inline void addd(Registers &registers, const uint16_t operand) {
	const uint16_t source = registers.reg<R16::D>();
	const uint16_t result = source + operand;

	registers.cc.set_nz(result);
	registers.cc.set<ConditionCode::Carry>(result < operand);
	registers.cc.set_overflow(result, source, operand);
	registers.reg<R16::D>() = result;
}

template <R8 r, bool with_carry, bool store_result>
void sub(Registers &registers, const uint8_t operand) {
	const uint8_t source = registers.reg<r>();
	const uint8_t result = source - operand - (with_carry ? registers.cc.carry() : 0);

	// Half carry is formally undefined after a subtract. This is a guess.
	const uint8_t half = (source & 0xf) + (~operand & 0xf) + (1 ^ (with_carry ? registers.cc.carry() : 0));
	registers.cc.set<ConditionCode::HalfCarry>(half & 0x10);

	registers.cc.set_nz(result);
	registers.cc.set_overflow(result, source, uint8_t(~operand));
	registers.cc.set<ConditionCode::Carry>(result > source || (with_carry && registers.cc.carry() && result >= source));

	if constexpr (store_result) registers.reg<r>() = result;
}

template <R16 r, bool store_result>
void sub(Registers &registers, const uint16_t operand) {
	const uint16_t source = registers.reg<r>();
	const uint16_t result = source - operand;

	registers.cc.set_nz(result);
	registers.cc.set_overflow(result, source, uint16_t(~operand));
	registers.cc.set<ConditionCode::Carry>(result > source);

	if constexpr (store_result) registers.reg<r>() = result;
}

inline void mul(Registers &registers) {
	const uint16_t result = registers.reg<R8::A>() * registers.reg<R8::B>();
	registers.reg<R16::D>() = result;
	registers.cc.set<ConditionCode::Zero>(!result);
	registers.cc.set<ConditionCode::Carry>(result & 0x8000);
}

inline void daa(Registers &registers) {
	const uint8_t original = registers.reg<R8::A>();
	uint8_t result = original;

	if(
		registers.cc.get<ConditionCode::HalfCarry>() ||
		(result & 0x0f) > 9
	) {
		result += 0x06;
	}
	if(
		registers.cc.get<ConditionCode::Carry>() ||
		(result >> 4) > 9 ||
		(original >> 4) > 9		// In case carry from above rolled the high digit over.
	) {
		result += 0x60;
	}

	registers.cc.set_nz(result);
	registers.cc.set<ConditionCode::Carry>(result < original);
	registers.cc.set<ConditionCode::Overflow>((original ^ result) & 0x80);
	registers.reg<R8::A>() = result;
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
	registers.cc.set<ConditionCode::Overflow>(((value << 1) ^ value) & 0x80);
	registers.cc.set<ConditionCode::Carry>(value >> 7);
	value <<= 1;
	registers.cc.set_nz(value);
}

template <R8 r>
void lsl(Registers &registers) {
	lsl(registers, registers.reg<r>());
}

inline void asr(Registers &registers, uint8_t &value) {
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
void lea(Registers &registers, const uint16_t operand) {
	registers.reg<r>() = operand;

	if constexpr (r == R16::X || r == R16::Y) {
		registers.cc.set<ConditionCode::Zero>(!operand);
	}
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
bool bra(Registers &registers, const OperandT operand) {
	if(!registers.cc.test<condition>()) {
		return false;
	}

	if constexpr (sizeof(OperandT) == 2) {
		registers.reg<R16::PC>() += operand;
	} else {
		registers.reg<R16::PC>() += int8_t(operand);
	}
	return true;
}

// MARK: - Dispatch.

/// Performs anything that doesn't involve calculating an effective address, manipulating the stack or affecting the ongoing flow of operation,
/// i.e. the subset of instructions that can be rationalised as not touching memory or having any knowledge of the instruction stream once
/// an appropriately-sized operand has been fetched.
///
/// The nop handler will be called each time a NOP is encountered in code. This is intended to provide for high-level triggers without active
/// instruction stream monitoring. A version that doesn't require a NOP handler is also provided below.
///
/// @returns The number of cycles spent in execution of this operation, abstract of the addressing mode in use. So EXG takes eight cycles,
/// of which the first two are the stuff of immediate addressing — fetching an opcode then fetching a post byte. So this function would return 6,
/// to indicate that processing the EXG additionally took six cycles.
template <typename NopHandlerT>
Cycles perform(
	const InstructionSet::M6809::Operation operation,
	Registers &registers,
	RegisterPair16 &operand,
	const NopHandlerT &&nop_handler
) {
	auto &byte = operand.halves.low;
	auto &word = operand.full;

	using Condition = InstructionSet::M6809::Condition;
	switch(operation) {
		using enum InstructionSet::M6809::Operation;

		case NOP:
			nop_handler();
			[[fallthrough]];
		case None:
		return 0;

		case ABX:	abx(registers);								return 1;

		case ADCA:	add<R8::A, true>(registers, byte);			return 0;
		case ADCB:	add<R8::B, true>(registers, byte);			return 0;
		case ADDA:	add<R8::A, false>(registers, byte);			return 0;
		case ADDB:	add<R8::B, false>(registers, byte);			return 0;
		case SBCA:	sub<R8::A, true, true>(registers, byte);	return 0;
		case SBCB:	sub<R8::B, true, true>(registers, byte);	return 0;
		case SUBA:	sub<R8::A, false, true>(registers, byte);	return 0;
		case SUBB:	sub<R8::B, false, true>(registers, byte);	return 0;
		case CMPA:	sub<R8::A, false, false>(registers, byte);	return 0;
		case CMPB:	sub<R8::B, false, false>(registers, byte);	return 0;

		case ADDD:	addd(registers, word);					return 1;
		case SUBD:	sub<R16::D, true>(registers, word);		return 1;
		case CMPX:	sub<R16::X, false>(registers, word);	return 1;
		case CMPD:	sub<R16::D, false>(registers, word);	return 1;
		case CMPY:	sub<R16::Y, false>(registers, word);	return 1;
		case CMPU:	sub<R16::U, false>(registers, word);	return 1;
		case CMPS:	sub<R16::S, false>(registers, word);	return 1;

		case DAA:	daa(registers);							return 0;

		case ANDA:	and_<R8::A>(registers, byte);			return 0;
		case ANDB:	and_<R8::B>(registers, byte);			return 0;
		case ORA:	or_<R8::A>(registers, byte);			return 0;
		case ORB:	or_<R8::B>(registers, byte);			return 0;
		case EORA:	eor_<R8::A>(registers, byte);			return 0;
		case EORB:	eor_<R8::B>(registers, byte);			return 0;

		case ANDCC:	and_<R8::CC>(registers, byte);			return 1;
		case ORCC:	or_<R8::CC>(registers, byte);			return 1;

		case INCA:	inc<R8::A>(registers);					return 0;
		case INCB:	inc<R8::B>(registers);					return 0;
		case INC:	inc(registers, byte);					return 0;
		case DECA:	dec<R8::A>(registers);					return 0;
		case DECB:	dec<R8::B>(registers);					return 0;
		case DEC:	dec(registers, byte);					return 0;

		case NEGA:	neg<R8::A>(registers);					return 0;
		case NEGB:	neg<R8::B>(registers);					return 0;
		case NEG:	neg(registers, byte);					return 0;
		case COMA:	com<R8::A>(registers);					return 0;
		case COMB:	com<R8::B>(registers);					return 0;
		case COM:	com(registers, byte);					return 0;

		case ASRA:	asr<R8::A>(registers);					return 0;
		case ASRB:	asr<R8::B>(registers);					return 0;
		case ASR:	asr(registers, byte);					return 0;
		case LSLA:	lsl<R8::A>(registers);					return 0;
		case LSLB:	lsl<R8::B>(registers);					return 0;
		case LSL:	lsl(registers, byte);					return 0;
		case LSRA:	lsr<R8::A>(registers);					return 0;
		case LSRB:	lsr<R8::B>(registers);					return 0;
		case LSR:	lsr(registers, byte);					return 0;
		case ROLA:	rol<R8::A>(registers);					return 0;
		case ROLB:	rol<R8::B>(registers);					return 0;
		case ROL:	rol(registers, byte);					return 0;
		case RORA:	ror<R8::A>(registers);					return 0;
		case RORB:	ror<R8::B>(registers);					return 0;
		case ROR:	ror(registers, byte);					return 0;

		case BCC:	bra<Condition::CC>(registers, byte);	return 1;
		case BCS:	bra<Condition::CS>(registers, byte);	return 1;
		case BEQ:	bra<Condition::EQ>(registers, byte);	return 1;
		case BGE:	bra<Condition::GE>(registers, byte);	return 1;
		case BGT:	bra<Condition::GT>(registers, byte);	return 1;
		case BHI:	bra<Condition::HI>(registers, byte);	return 1;
		case BLE:	bra<Condition::LE>(registers, byte);	return 1;
		case BLS:	bra<Condition::LS>(registers, byte);	return 1;
		case BLT:	bra<Condition::LT>(registers, byte);	return 1;
		case BMI:	bra<Condition::MI>(registers, byte);	return 1;
		case BNE:	bra<Condition::NE>(registers, byte);	return 1;
		case BPL:	bra<Condition::PL>(registers, byte);	return 1;
		case BRA:	bra<Condition::A>(registers, byte);		return 1;
		case BRN:	bra<Condition::N>(registers, byte);		return 1;
		case BVC:	bra<Condition::VC>(registers, byte);	return 1;
		case BVS:	bra<Condition::VS>(registers, byte);	return 1;

		case BITA:	bit<R8::A>(registers, byte);			return 0;
		case BITB:	bit<R8::B>(registers, byte);			return 0;

		case CLRA:	clr<R8::A>(registers);					return 0;
		case CLRB:	clr<R8::B>(registers);					return 0;
		case CLR:	clr(registers, byte);					return 0;

		case LBCC:	return bra<Condition::CC>(registers, word) ? 2 : 1;
		case LBCS:	return bra<Condition::CS>(registers, word) ? 2 : 1;
		case LBEQ:	return bra<Condition::EQ>(registers, word) ? 2 : 1;
		case LBGE:	return bra<Condition::GE>(registers, word) ? 2 : 1;
		case LBGT:	return bra<Condition::GT>(registers, word) ? 2 : 1;
		case LBHI:	return bra<Condition::HI>(registers, word) ? 2 : 1;
		case LBLE:	return bra<Condition::LE>(registers, word) ? 2 : 1;
		case LBLS:	return bra<Condition::LS>(registers, word) ? 2 : 1;
		case LBLT:	return bra<Condition::LT>(registers, word) ? 2 : 1;
		case LBMI:	return bra<Condition::MI>(registers, word) ? 2 : 1;
		case LBNE:	return bra<Condition::NE>(registers, word) ? 2 : 1;
		case LBPL:	return bra<Condition::PL>(registers, word) ? 2 : 1;
		case LBRA:	return bra<Condition::A>(registers, word) ? 2 : 1;
		case LBRN:	return bra<Condition::N>(registers, word) ? 2 : 1;
		case LBVC:	return bra<Condition::VC>(registers, word) ? 2 : 1;
		case LBVS:	return bra<Condition::VS>(registers, word) ? 2 : 1;

		case LDA:	ld<R8::A>(registers, byte);				return 0;
		case LDB:	ld<R8::B>(registers, byte);				return 0;
		case STA:	st<R8::A>(registers, byte);				return 0;
		case STB:	st<R8::B>(registers, byte);				return 0;
		case LDD:	ld<R16::D>(registers, word);			return 0;
		case LDU:	ld<R16::U>(registers, word);			return 0;
		case LDX:	ld<R16::X>(registers, word);			return 0;
		case LDY:	ld<R16::Y>(registers, word);			return 0;
		case LDS:	ld<R16::S>(registers, word);			return 0;
		case STD:	st<R16::D>(registers, word);			return 0;
		case STU:	st<R16::U>(registers, word);			return 0;
		case STX:	st<R16::X>(registers, word);			return 0;
		case STY:	st<R16::Y>(registers, word);			return 0;
		case STS:	st<R16::S>(registers, word);			return 0;

		case LEAU:	lea<R16::U>(registers, word);			return 1;
		case LEAX:	lea<R16::X>(registers, word);			return 1;
		case LEAY:	lea<R16::Y>(registers, word);			return 1;
		case LEAS:	lea<R16::S>(registers, word);			return 1;

		case MUL:	mul(registers);							return 9;	// Per 6809cyc.txt; might need more research.

		case TFR:	tfr(registers, byte);					return 4;
		case EXG:	exg(registers, byte);					return 6;

		case SEX:	sex(registers);							return 0;
		case TSTA:	tst<R8::A>(registers);					return 0;
		case TSTB:	tst<R8::B>(registers);					return 0;
		case TST:	tst(registers, byte);					return 2;	// Weird, but seemingly true (?)

		case JMP:	registers.pc.full = word;				return 0;

		// Flow control that requires stack access.
		case JSR:	case BSR:	case LBSR:
		case RTI:	case RTS:
		case SWI:	case SWI2:	case SWI3:
		case SYNC:	case RESET:	case CWAI:

		// Stack access.
		case PSHS:	case PULS:	case PSHU:	case PULU:

		// Operation selection.
		case Page1:	case Page2:

		default: __builtin_unreachable();
	}
}

/// Acts as @c perform above, but doesn't require the caller to provide a NOP handler.
inline Cycles perform(
	const InstructionSet::M6809::Operation operation,
	Registers &registers,
	RegisterPair16 &operand
) {
	return perform(operation, registers, operand, []{});
}

/// @returns @c true if no internal cycles are required to perform `operation`.
inline bool is_zero_costed(const InstructionSet::M6809::Operation operation) {
	switch(operation) {
		using enum InstructionSet::M6809::Operation;
		default: return false;

		case NOP:	case None:	case ADCA:	case ADCB:	case ADDA:
		case ADDB:	case SBCA:	case SBCB:	case SUBA:	case SUBB:
		case CMPA:	case CMPB:	case DAA:	case ANDA:	case ANDB:
		case ORA:	case ORB:	case EORA:	case EORB:	case INCA:
		case INCB:	case INC:	case DECA:	case DECB:	case DEC:
		case NEGA:	case NEGB:	case NEG:	case COMA:	case COMB:
		case COM:	case ASRA:	case ASRB:	case ASR:	case LSLA:
		case LSLB:	case LSL:	case LSRA:	case LSRB:	case LSR:
		case ROLA:	case ROLB:	case ROL:	case RORA:	case RORB:
		case ROR:	case BITA:	case BITB:	case CLRA:	case CLRB:
		case CLR:	case LDA:	case LDB:	case STA:	case STB:
		case LDD:	case LDU:	case LDX:	case LDY:	case LDS:
		case STD:	case STU:	case STX:	case STY:	case STS:
		case LEAU:	case LEAX:	case LEAY:	case LEAS:	case SEX:
		case TSTA:	case TSTB:	case JMP:
			return true;
	}
}
}
