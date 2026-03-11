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

template <R8 r>
inline void ld(Registers &registers, const uint8_t operand) {
	registers.reg<r>() = operand;
	registers.cc.set_nz(operand);
	registers.cc.set<ConditionCode::Overflow>(false);
}

template <R16 r>
inline void ld(Registers &registers, const uint16_t operand) {
	registers.reg<r>() = operand;
	registers.cc.set_nz(operand >> 8);
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

inline void perform(const InstructionSet::M6809::Operation operation, Registers &registers, const uint16_t operand) {
	switch(operation) {
		using enum InstructionSet::M6809::Operation;

		case None:	break;

		case LDA:	ld<R8::A>(registers, uint8_t(operand));	break;
		case LDB:	ld<R8::B>(registers, uint8_t(operand));	break;

		case LDD:	ld<R16::D>(registers, operand);		break;
		case LDU:	ld<R16::U>(registers, operand);		break;
		case LDX:	ld<R16::X>(registers, operand);		break;
		case LDY:	ld<R16::Y>(registers, operand);		break;
		case LDS:	ld<R16::S>(registers, operand);		break;

		case TFR:	tfr(registers, uint8_t(operand));	break;
		case EXG:	exg(registers, uint8_t(operand));	break;

		default: __builtin_unreachable();
	}
}

}
