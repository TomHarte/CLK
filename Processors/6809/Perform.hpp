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

inline void perform(const InstructionSet::M6809::Operation operation, Registers &registers, const uint16_t operand) {
	switch(operation) {
		using enum InstructionSet::M6809::Operation;

		case LDA:	ld<R8::A>(registers, uint8_t(operand));	break;
		case LDB:	ld<R8::B>(registers, uint8_t(operand));	break;

		case LDD:	ld<R16::D>(registers, operand);	break;
		case LDU:	ld<R16::U>(registers, operand);	break;
		case LDX:	ld<R16::X>(registers, operand);	break;
		case LDY:	ld<R16::Y>(registers, operand);	break;
		case LDS:	ld<R16::S>(registers, operand);	break;

		default: __builtin_unreachable();
	}
}

}
