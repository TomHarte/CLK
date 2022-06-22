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
		case Register::ProgramCounter:			return registers_.pc;
		case Register::LastOperationAddress:	return last_operation_pc_;
		case Register::StackPointer:
			return
				(registers_.s.full & (registers_.emulation_flag ? 0xff : 0xffff)) |
				(registers_.emulation_flag ? 0x100 : 0x000);
		case Register::Flags:					return get_flags();
		case Register::A:						return registers_.a.full;
		case Register::X:						return registers_.x.full;
		case Register::Y:						return registers_.y.full;
		case Register::EmulationFlag:			return registers_.emulation_flag;
		case Register::DataBank:				return registers_.data_bank >> 16;
		case Register::ProgramBank:				return registers_.program_bank >> 16;
		case Register::Direct:					return registers_.direct;
		default: return 0;
	}
}

void ProcessorBase::set_value_of_register(Register r, uint16_t value) {
	switch (r) {
		case Register::ProgramCounter:	registers_.pc = value;									break;
		case Register::StackPointer:	registers_.s.full = value;								break;
		case Register::Flags:			set_flags(uint8_t(value));								break;
		case Register::A:				registers_.a.full = value;								break;
		case Register::X:				registers_.x.full = value & registers_.x_mask;			break;
		case Register::Y:				registers_.y.full = value & registers_.x_mask;			break;
		case Register::EmulationFlag:	set_emulation_mode(value);								break;
		case Register::DataBank:		registers_.data_bank = uint32_t(value & 0xff) << 16;	break;
		case Register::ProgramBank:		registers_.program_bank = uint32_t(value &0xff) << 16;	break;
		case Register::Direct:			registers_.direct = value;								break;
		default: break;
	}
}
