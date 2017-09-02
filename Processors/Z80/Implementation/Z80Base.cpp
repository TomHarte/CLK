//
//  Z80Storage.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 01/09/2017.
//  Copyright © 2017 Thomas Harte. All rights reserved.
//

#include "../Z80.hpp"

using namespace CPU::Z80;

ProcessorStorage::ProcessorStorage() :
			halt_mask_(0xff),
			interrupt_mode_(0),
			wait_line_(false),
			request_status_(Interrupt::PowerOn),
			last_request_status_(Interrupt::PowerOn),
			irq_line_(false),
			nmi_line_(false),
			bus_request_line_(false),
			pc_increment_(1),
			scheduled_program_counter_(nullptr) {
	set_flags(0xff);
}

void ProcessorStorage::install_default_instruction_set() {
	MicroOp conditional_call_untaken_program[] = Sequence(ReadInc(pc_, temp16_.bytes.high));
	copy_program(conditional_call_untaken_program, conditional_call_untaken_program_);

	assemble_base_page(base_page_, hl_, false, cb_page_);
	assemble_base_page(dd_page_, ix_, true, ddcb_page_);
	assemble_base_page(fd_page_, iy_, true, fdcb_page_);
	assemble_ed_page(ed_page_);

	fdcb_page_.r_step = 0;
	fd_page_.is_indexed = true;
	fdcb_page_.is_indexed = true;

	ddcb_page_.r_step = 0;
	dd_page_.is_indexed = true;
	ddcb_page_.is_indexed = true;

	assemble_fetch_decode_execute(base_page_, 4);
	assemble_fetch_decode_execute(dd_page_, 4);
	assemble_fetch_decode_execute(fd_page_, 4);
	assemble_fetch_decode_execute(ed_page_, 4);
	assemble_fetch_decode_execute(cb_page_, 4);

	assemble_fetch_decode_execute(fdcb_page_, 3);
	assemble_fetch_decode_execute(ddcb_page_, 3);

	MicroOp reset_program[] = Sequence(InternalOperation(6), {MicroOp::Reset});

	// Justification for NMI timing: per Wilf Rigter on the ZX81 (http://www.user.dccnet.com/wrigter/index_files/ZX81WAIT.htm),
	// wait cycles occur between T2 and T3 during NMI; extending the refresh cycle is also consistent with my guess
	// for the action of other non-four-cycle opcode fetches
	MicroOp nmi_program[] = {
		{ MicroOp::BeginNMI },
		BusOp(ReadOpcodeStart()),
		BusOp(ReadOpcodeWait(true)),
		BusOp(ReadOpcodeEnd()),
		BusOp(Refresh(6)),
		Push(pc_),
		{ MicroOp::JumpTo66, nullptr, nullptr},
		{ MicroOp::MoveToNextProgram }
	};
	MicroOp irq_mode0_program[] = {
		{ MicroOp::BeginIRQMode0 },
		BusOp(IntAckStart(5, operation_)),
		BusOp(IntWait(operation_)),
		BusOp(IntAckEnd(operation_)),
		{ MicroOp::DecodeOperationNoRChange }
	};
	MicroOp irq_mode1_program[] = {
		{ MicroOp::BeginIRQ },
		BusOp(IntAckStart(7, operation_)),	// 7 half cycles (including  +
		BusOp(IntWait(operation_)),			// [potentially 2 half cycles] +
		BusOp(IntAckEnd(operation_)),		// Implicitly 3 half cycles +
		BusOp(Refresh(4)),					// 4 half cycles +
		Push(pc_),							// 12 half cycles = 26 half cycles = 13 cycles
		{ MicroOp::Move16, &temp16_.full, &pc_.full },
		{ MicroOp::MoveToNextProgram }
	};
	MicroOp irq_mode2_program[] = {
		{ MicroOp::BeginIRQ },
		BusOp(IntAckStart(7, temp16_.bytes.low)),
		BusOp(IntWait(temp16_.bytes.low)),
		BusOp(IntAckEnd(temp16_.bytes.low)),
		BusOp(Refresh(4)),
		Push(pc_),
		{ MicroOp::Move8, &ir_.bytes.high, &temp16_.bytes.high },
		Read16(temp16_, pc_),
		{ MicroOp::MoveToNextProgram }
	};

	copy_program(reset_program, reset_program_);
	copy_program(nmi_program, nmi_program_);
	copy_program(irq_mode0_program, irq_program_[0]);
	copy_program(irq_mode1_program, irq_program_[1]);
	copy_program(irq_mode2_program, irq_program_[2]);
}

void ProcessorBase::reset_power_on() {
	request_status_ &= ~Interrupt::PowerOn;
	last_request_status_ &= ~Interrupt::PowerOn;
}

uint16_t ProcessorBase::get_value_of_register(Register r) {
	switch (r) {
		case Register::ProgramCounter:			return pc_.full;
		case Register::StackPointer:			return sp_.full;

		case Register::A:						return a_;
		case Register::Flags:					return get_flags();
		case Register::AF:						return (uint16_t)((a_ << 8) | get_flags());
		case Register::B:						return bc_.bytes.high;
		case Register::C:						return bc_.bytes.low;
		case Register::BC:						return bc_.full;
		case Register::D:						return de_.bytes.high;
		case Register::E:						return de_.bytes.low;
		case Register::DE:						return de_.full;
		case Register::H:						return hl_.bytes.high;
		case Register::L:						return hl_.bytes.low;
		case Register::HL:						return hl_.full;

		case Register::ADash:					return afDash_.bytes.high;
		case Register::FlagsDash:				return afDash_.bytes.low;
		case Register::AFDash:					return afDash_.full;
		case Register::BDash:					return bcDash_.bytes.high;
		case Register::CDash:					return bcDash_.bytes.low;
		case Register::BCDash:					return bcDash_.full;
		case Register::DDash:					return deDash_.bytes.high;
		case Register::EDash:					return deDash_.bytes.low;
		case Register::DEDash:					return deDash_.full;
		case Register::HDash:					return hlDash_.bytes.high;
		case Register::LDash:					return hlDash_.bytes.low;
		case Register::HLDash:					return hlDash_.full;

		case Register::IXh:						return ix_.bytes.high;
		case Register::IXl:						return ix_.bytes.low;
		case Register::IX:						return ix_.full;
		case Register::IYh:						return iy_.bytes.high;
		case Register::IYl:						return iy_.bytes.low;
		case Register::IY:						return iy_.full;

		case Register::R:						return ir_.bytes.low;
		case Register::I:						return ir_.bytes.high;
		case Register::Refresh:					return ir_.full;

		case Register::IFF1:					return iff1_ ? 1 : 0;
		case Register::IFF2:					return iff2_ ? 1 : 0;
		case Register::IM:						return (uint16_t)interrupt_mode_;

		case Register::MemPtr:					return memptr_.full;

		default: return 0;
	}
}

void ProcessorBase::set_value_of_register(Register r, uint16_t value) {
	switch (r) {
		case Register::ProgramCounter:	pc_.full = value;				break;
		case Register::StackPointer:	sp_.full = value;				break;

		case Register::A:				a_ = (uint8_t)value;			break;
		case Register::AF:				a_ = (uint8_t)(value >> 8);		// deliberate fallthrough...
		case Register::Flags:			set_flags((uint8_t)value);		break;

		case Register::B:				bc_.bytes.high = (uint8_t)value;		break;
		case Register::C:				bc_.bytes.low = (uint8_t)value;			break;
		case Register::BC:				bc_.full = value;						break;
		case Register::D:				de_.bytes.high = (uint8_t)value;		break;
		case Register::E:				de_.bytes.low = (uint8_t)value;			break;
		case Register::DE:				de_.full = value;						break;
		case Register::H:				hl_.bytes.high = (uint8_t)value;		break;
		case Register::L:				hl_.bytes.low = (uint8_t)value;			break;
		case Register::HL:				hl_.full = value;						break;

		case Register::ADash:			afDash_.bytes.high = (uint8_t)value;	break;
		case Register::FlagsDash:		afDash_.bytes.low = (uint8_t)value;		break;
		case Register::AFDash:			afDash_.full = value;					break;
		case Register::BDash:			bcDash_.bytes.high = (uint8_t)value;	break;
		case Register::CDash:			bcDash_.bytes.low = (uint8_t)value;		break;
		case Register::BCDash:			bcDash_.full = value;					break;
		case Register::DDash:			deDash_.bytes.high = (uint8_t)value;	break;
		case Register::EDash:			deDash_.bytes.low = (uint8_t)value;		break;
		case Register::DEDash:			deDash_.full = value;					break;
		case Register::HDash:			hlDash_.bytes.high = (uint8_t)value;	break;
		case Register::LDash:			hlDash_.bytes.low = (uint8_t)value;		break;
		case Register::HLDash:			hlDash_.full = value;					break;

		case Register::IXh:				ix_.bytes.high = (uint8_t)value;		break;
		case Register::IXl:				ix_.bytes.low = (uint8_t)value;			break;
		case Register::IX:				ix_.full = value;						break;
		case Register::IYh:				iy_.bytes.high = (uint8_t)value;		break;
		case Register::IYl:				iy_.bytes.low = (uint8_t)value;			break;
		case Register::IY:				iy_.full = value;						break;

		case Register::R:				ir_.bytes.low = (uint8_t)value;			break;
		case Register::I:				ir_.bytes.high = (uint8_t)value;		break;
		case Register::Refresh:			ir_.full = (uint16_t)value;				break;

		case Register::IFF1:			iff1_ = !!value;						break;
		case Register::IFF2:			iff2_ = !!value;						break;
		case Register::IM:				interrupt_mode_ = value % 3;			break;

		case Register::MemPtr:			memptr_.full = value;					break;

		default: break;
	}
}

PartialMachineCycle::PartialMachineCycle(const PartialMachineCycle &rhs) :
	operation(rhs.operation),
	length(rhs.length),
	address(rhs.address),
	value(rhs.value),
	was_requested(rhs.was_requested) {}
PartialMachineCycle::PartialMachineCycle(Operation operation, HalfCycles length, uint16_t *address, uint8_t *value, bool was_requested) noexcept :
	operation(operation), length(length), address(address), value(value), was_requested(was_requested)  {}
PartialMachineCycle::PartialMachineCycle() noexcept :
	operation(Internal), length(0), address(nullptr), value(nullptr), was_requested(false) {}
