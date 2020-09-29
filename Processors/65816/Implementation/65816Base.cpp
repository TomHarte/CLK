//
//  65816Base.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 28/09/2020.
//  Copyright © 2020 Thomas Harte. All rights reserved.
//

#include "../65816.hpp"

using namespace CPU::WDC65816;

uint16_t ProcessorBase::get_value_of_register(Register r) const {
	switch (r) {
		case Register::ProgramCounter:			return pc_;
		case Register::LastOperationAddress:	return last_operation_pc_;
		case Register::StackPointer:			return s_;
//		case Register::Flags:					return get_flags();
		case Register::A:						return a_.full;
		case Register::X:						return x_.full;
		case Register::Y:						return y_.full;
		default: return 0;
	}
}

void ProcessorBase::set_value_of_register(Register r, uint16_t value) {
	switch (r) {
		case Register::ProgramCounter:	pc_ = value;				break;
		case Register::StackPointer:	s_ = value;					break;
//		case Register::Flags:			set_flags(uint8_t(value));	break;
		case Register::A:				a_ = value;					break;
		case Register::X:				x_ = value;					break;
		case Register::Y:				y_ = value;					break;
		default: break;
	}
}
