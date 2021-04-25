//
//  Z80Storage.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 01/09/2017.
//  Copyright 2017 Thomas Harte. All rights reserved.
//

#include "../Z80.hpp"

using namespace CPU::Z80;

void ProcessorBase::reset_power_on() {
	request_status_ &= ~Interrupt::PowerOn;
	last_request_status_ &= ~Interrupt::PowerOn;
}

uint16_t ProcessorBase::get_value_of_register(Register r) const {
	switch (r) {
		case Register::ProgramCounter:			return pc_.full;
		case Register::StackPointer:			return sp_.full;

		case Register::A:						return a_;
		case Register::Flags:					return get_flags();
		case Register::AF:						return uint16_t((a_ << 8) | get_flags());
		case Register::B:						return bc_.halves.high;
		case Register::C:						return bc_.halves.low;
		case Register::BC:						return bc_.full;
		case Register::D:						return de_.halves.high;
		case Register::E:						return de_.halves.low;
		case Register::DE:						return de_.full;
		case Register::H:						return hl_.halves.high;
		case Register::L:						return hl_.halves.low;
		case Register::HL:						return hl_.full;

		case Register::ADash:					return af_dash_.halves.high;
		case Register::FlagsDash:				return af_dash_.halves.low;
		case Register::AFDash:					return af_dash_.full;
		case Register::BDash:					return bc_dash_.halves.high;
		case Register::CDash:					return bc_dash_.halves.low;
		case Register::BCDash:					return bc_dash_.full;
		case Register::DDash:					return de_dash_.halves.high;
		case Register::EDash:					return de_dash_.halves.low;
		case Register::DEDash:					return de_dash_.full;
		case Register::HDash:					return hl_dash_.halves.high;
		case Register::LDash:					return hl_dash_.halves.low;
		case Register::HLDash:					return hl_dash_.full;

		case Register::IXh:						return ix_.halves.high;
		case Register::IXl:						return ix_.halves.low;
		case Register::IX:						return ix_.full;
		case Register::IYh:						return iy_.halves.high;
		case Register::IYl:						return iy_.halves.low;
		case Register::IY:						return iy_.full;

		case Register::R:						return ir_.halves.low;
		case Register::I:						return ir_.halves.high;
		case Register::Refresh:					return ir_.full;

		case Register::IFF1:					return iff1_ ? 1 : 0;
		case Register::IFF2:					return iff2_ ? 1 : 0;
		case Register::IM:						return uint16_t(interrupt_mode_);

		case Register::MemPtr:					return memptr_.full;

		default: return 0;
	}
}

void ProcessorBase::set_value_of_register(Register r, uint16_t value) {
	switch (r) {
		case Register::ProgramCounter:	pc_.full = value;				break;
		case Register::StackPointer:	sp_.full = value;				break;

		case Register::A:				a_ = uint8_t(value);			break;
		case Register::AF:				a_ = uint8_t(value >> 8);		[[fallthrough]];
		case Register::Flags:			set_flags(uint8_t(value));		break;

		case Register::B:				bc_.halves.high = uint8_t(value);	break;
		case Register::C:				bc_.halves.low = uint8_t(value);	break;
		case Register::BC:				bc_.full = value;					break;
		case Register::D:				de_.halves.high = uint8_t(value);	break;
		case Register::E:				de_.halves.low = uint8_t(value);	break;
		case Register::DE:				de_.full = value;					break;
		case Register::H:				hl_.halves.high = uint8_t(value);	break;
		case Register::L:				hl_.halves.low = uint8_t(value);	break;
		case Register::HL:				hl_.full = value;					break;

		case Register::ADash:			af_dash_.halves.high = uint8_t(value);	break;
		case Register::FlagsDash:		af_dash_.halves.low = uint8_t(value);	break;
		case Register::AFDash:			af_dash_.full = value;					break;
		case Register::BDash:			bc_dash_.halves.high = uint8_t(value);	break;
		case Register::CDash:			bc_dash_.halves.low = uint8_t(value);	break;
		case Register::BCDash:			bc_dash_.full = value;					break;
		case Register::DDash:			de_dash_.halves.high = uint8_t(value);	break;
		case Register::EDash:			de_dash_.halves.low = uint8_t(value);	break;
		case Register::DEDash:			de_dash_.full = value;					break;
		case Register::HDash:			hl_dash_.halves.high = uint8_t(value);	break;
		case Register::LDash:			hl_dash_.halves.low = uint8_t(value);	break;
		case Register::HLDash:			hl_dash_.full = value;					break;

		case Register::IXh:				ix_.halves.high = uint8_t(value);		break;
		case Register::IXl:				ix_.halves.low = uint8_t(value);		break;
		case Register::IX:				ix_.full = value;						break;
		case Register::IYh:				iy_.halves.high = uint8_t(value);		break;
		case Register::IYl:				iy_.halves.low = uint8_t(value);		break;
		case Register::IY:				iy_.full = value;						break;

		case Register::R:				ir_.halves.low = uint8_t(value);		break;
		case Register::I:				ir_.halves.high = uint8_t(value);		break;
		case Register::Refresh:			ir_.full = value;						break;

		case Register::IFF1:			iff1_ = !!value;				break;
		case Register::IFF2:			iff2_ = !!value;				break;
		case Register::IM:				interrupt_mode_ = value % 3;	break;

		case Register::MemPtr:			memptr_.full = value;			break;

		default: break;
	}
}
