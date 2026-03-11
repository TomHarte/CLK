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

inline void abx(Registers &registers) {
	registers.reg<R16::X>() += registers.reg<R8::B>();
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

// MARK: - Data Transfer.

template <R8 r>
void ld(Registers &registers, const uint8_t operand) {
	registers.reg<r>() = operand;
	registers.cc.set_nz(operand);
	registers.cc.set<ConditionCode::Overflow>(false);
}

template <R16 r>
void ld(Registers &registers, const uint16_t operand) {
	registers.reg<r>() = operand;
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

// MARK: - Dispatch.

inline void perform(const InstructionSet::M6809::Operation operation, Registers &registers, RegisterPair16 &operand) {
	auto &byte = operand.halves.low;
	auto &word = operand.full;

	switch(operation) {
		using enum InstructionSet::M6809::Operation;

		case None:	break;

		case ABX:	abx(registers);						break;
		case ADCA:	add<R8::A, true>(registers, byte);	break;
		case ADCB:	add<R8::B, true>(registers, byte);	break;
		case ADDA:	add<R8::A, false>(registers, byte);	break;
		case ADDB:	add<R8::B, false>(registers, byte);	break;
		case ADDD:	addd(registers, word);				break;
		case ANDA:	and_<R8::A>(registers, byte);		break;
		case ANDB:	and_<R8::B>(registers, byte);		break;
		case ANDCC:	and_<R8::CC>(registers, byte);		break;

		case ASRA:	asr<R8::A>(registers);				break;
		case ASRB:	asr<R8::B>(registers);				break;
		case ASR:	asr(registers, byte);				break;
		case LSLA:	lsl<R8::A>(registers);				break;
		case LSLB:	lsl<R8::B>(registers);				break;
		case LSL:	lsl(registers, byte);				break;
		case LSRA:	lsr<R8::A>(registers);				break;
		case LSRB:	lsr<R8::B>(registers);				break;
		case LSR:	lsr(registers, byte);				break;

		case LDA:	ld<R8::A>(registers, byte);			break;
		case LDB:	ld<R8::B>(registers, byte);			break;

		case LDD:	ld<R16::D>(registers, word);		break;
		case LDU:	ld<R16::U>(registers, word);		break;
		case LDX:	ld<R16::X>(registers, word);		break;
		case LDY:	ld<R16::Y>(registers, word);		break;
		case LDS:	ld<R16::S>(registers, word);		break;

		case TFR:	tfr(registers, byte);				break;
		case EXG:	exg(registers, byte);				break;

		default: __builtin_unreachable();
	}
}

}
