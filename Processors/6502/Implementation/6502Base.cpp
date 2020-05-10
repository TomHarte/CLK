//
//  6502Base.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 31/08/2017.
//  Copyright 2017 Thomas Harte. All rights reserved.
//

#include "../6502.hpp"

#include <cassert>

using namespace CPU::MOS6502;

const uint8_t CPU::MOS6502::JamOpcode = 0xf2;

uint16_t ProcessorBase::get_value_of_register(Register r) const {
	switch (r) {
		case Register::ProgramCounter:			return pc_.full;
		case Register::LastOperationAddress:	return last_operation_pc_.full;
		case Register::StackPointer:			return s_;
		case Register::Flags:					return get_flags();
		case Register::A:						return a_;
		case Register::X:						return x_;
		case Register::Y:						return y_;
		default: return 0;
	}
}

void ProcessorBase::set_value_of_register(Register r, uint16_t value) {
	switch (r) {
		case Register::ProgramCounter:	pc_.full = value;			break;
		case Register::StackPointer:	s_ = uint8_t(value);		break;
		case Register::Flags:			set_flags(uint8_t(value));	break;
		case Register::A:				a_ = uint8_t(value);		break;
		case Register::X:				x_ = uint8_t(value);		break;
		case Register::Y:				y_ = uint8_t(value);		break;
		default: break;
	}
}

bool ProcessorBase::is_jammed() const {
	return is_jammed_;
}
