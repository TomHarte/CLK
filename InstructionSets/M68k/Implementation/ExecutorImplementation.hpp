//
//  ExecutorImplementation.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 01/05/2022.
//  Copyright © 2022 Thomas Harte. All rights reserved.
//

#ifndef InstructionSets_M68k_ExecutorImplementation_hpp
#define InstructionSets_M68k_ExecutorImplementation_hpp

#include "../Perform.hpp"
#include "../ExceptionVectors.hpp"

#include <cassert>

namespace InstructionSet {
namespace M68k {

#define An(x)	registers_[8 + x]
#define Dn(x)	registers_[x]
#define sp		An(7)

#define AccessException(code, address, vector)	\
	uint64_t(((vector) << 8) | uint64_t(code) | ((address) << 16))

template <Model model, typename BusHandler>
Executor<model, BusHandler>::Executor(BusHandler &handler) : bus_handler_(handler) {
	reset_processor();
}

template <Model model, typename BusHandler>
void Executor<model, BusHandler>::reset_processor() {
	// Establish: supervisor state, all interrupts blocked.
	status_.set_status(0b0010'0011'1000'0000);
	did_update_status();

	// Seed stack pointer and program counter.
	sp.l = read<uint32_t>(0) & 0xffff'fffe;
	program_counter_.l = read<uint32_t>(4);
}

template <Model model, typename BusHandler>
template <typename IntT>
IntT Executor<model, BusHandler>::read(uint32_t address, bool is_from_pc) {
	const auto code = FunctionCode((active_stack_pointer_ << 2) | 1 << int(is_from_pc));
	if(model == Model::M68000 && sizeof(IntT) > 1 && address & 1) {
		throw AccessException(code, address, Exception::AddressError | (int(is_from_pc) << 3) | (1 << 4));
	}

	// TODO: omit generation of the FunctionCode if the BusHandler doesn't receive it.
	return bus_handler_.template read<IntT>(address, code);
}

template <Model model, typename BusHandler>
template <typename IntT>
void Executor<model, BusHandler>::write(uint32_t address, IntT value) {
	const auto code = FunctionCode((active_stack_pointer_ << 2) | 1);
	if(model == Model::M68000 && sizeof(IntT) > 1 && address & 1) {
		throw AccessException(code, address, Exception::AddressError);
	}

	bus_handler_.template write<IntT>(address, value, code);
}

template <Model model, typename BusHandler>
void Executor<model, BusHandler>::read(DataSize size, uint32_t address, CPU::SlicedInt32 &value) {
	switch(size) {
		case DataSize::Byte:		value.b = read<uint8_t>(address);	break;
		case DataSize::Word:		value.w = read<uint16_t>(address);	break;
		case DataSize::LongWord:	value.l = read<uint32_t>(address);	break;
	}
}

template <Model model, typename BusHandler>
void Executor<model, BusHandler>::write(DataSize size, uint32_t address, CPU::SlicedInt32 value) {
	switch(size) {
		case DataSize::Byte:		write<uint8_t>(address, value.b);	break;
		case DataSize::Word:		write<uint16_t>(address, value.w);	break;
		case DataSize::LongWord:	write<uint32_t>(address, value.l);	break;
	}
}

template <Model model, typename BusHandler>
template <typename IntT> IntT Executor<model, BusHandler>::read_pc() {
	const IntT result = read<IntT>(program_counter_.l, true);

	if constexpr (sizeof(IntT) == 4) {
		program_counter_.l += 4;
	} else {
		program_counter_.l += 2;
	}

	return result;
}

template <Model model, typename BusHandler>
uint32_t Executor<model, BusHandler>::index_8bitdisplacement() {
	// TODO: if not a 68000, check bit 8 for whether this should be a full extension word;
	// also include the scale field even if not.
	const auto extension = read_pc<uint16_t>();
	const auto offset = int8_t(extension);
	const int register_index = (extension >> 12) & 7;
	const uint32_t displacement = registers_[register_index + ((extension >> 12) & 0x08)].l;
	const uint32_t sized_displacement = (extension & 0x800) ? displacement : int16_t(displacement);
	return offset + sized_displacement;
}

template <Model model, typename BusHandler>
typename Executor<model, BusHandler>::EffectiveAddress Executor<model, BusHandler>::calculate_effective_address(Preinstruction instruction, uint16_t opcode, int index) {
	EffectiveAddress ea;

	switch(instruction.mode(index)) {
		case AddressingMode::None:
			// Permit an uninitialised effective address to be returned;
			// this value shouldn't be used.
		break;

		//
		// Operands that don't have effective addresses, which are returned as values.
		//
		case AddressingMode::DataRegisterDirect:
			ea.value = Dn(instruction.reg(index));
			ea.requires_fetch = false;
		break;
		case AddressingMode::AddressRegisterDirect:
			ea.value = An(instruction.reg(index));
			ea.requires_fetch = false;
		break;
		case AddressingMode::Quick:
			ea.value.l = quick(opcode, instruction.operation);
			ea.requires_fetch = false;
		break;
		case AddressingMode::ImmediateData:
			switch(instruction.operand_size()) {
				case DataSize::Byte:
					ea.value.l = read_pc<uint16_t>() & 0xff;
				break;
				case DataSize::Word:
					ea.value.l = read_pc<uint16_t>();
				break;
				case DataSize::LongWord:
					ea.value.l = read_pc<uint32_t>();
				break;
			}
			ea.requires_fetch = false;
		break;

		//
		// Absolute addresses.
		//
		case AddressingMode::AbsoluteShort:
			ea.value.l = int16_t(read_pc<uint16_t>());
			ea.requires_fetch = true;
		break;
		case AddressingMode::AbsoluteLong:
			ea.value.l = read_pc<uint32_t>();
			ea.requires_fetch = true;
		break;

		//
		// Address register indirects.
		//
		case AddressingMode::AddressRegisterIndirect:
			ea.value = An(instruction.reg(index));
			ea.requires_fetch = true;
		break;
		case AddressingMode::AddressRegisterIndirectWithPostincrement: {
			const auto reg = instruction.reg(index);

			ea.value = An(reg);
			ea.requires_fetch = true;

			switch(instruction.operand_size()) {
				case DataSize::Byte:		An(reg).l += byte_increments[reg];	break;
				case DataSize::Word:		An(reg).l += 2;						break;
				case DataSize::LongWord:	An(reg).l += 4;						break;
			}
		} break;
		case AddressingMode::AddressRegisterIndirectWithPredecrement: {
			const auto reg = instruction.reg(index);

			switch(instruction.operand_size()) {
				case DataSize::Byte:		An(reg).l -= byte_increments[reg];	break;
				case DataSize::Word:		An(reg).l -= 2;						break;
				case DataSize::LongWord:	An(reg).l -= 4;						break;
			}

			ea.value = An(reg);
			ea.requires_fetch = true;
		} break;
		case AddressingMode::AddressRegisterIndirectWithDisplacement:
			ea.value.l = An(instruction.reg(index)).l + int16_t(read_pc<uint16_t>());
			ea.requires_fetch = true;
		break;
		case AddressingMode::AddressRegisterIndirectWithIndex8bitDisplacement:
			ea.value.l = An(instruction.reg(index)).l + index_8bitdisplacement();
			ea.requires_fetch = true;
		break;

		//
		// PC-relative addresses.
		//
		// TODO: rephrase these in terms of instruction_address_. Just for security
		// against whatever mutations the PC has been through already to get to here.
		//
		case AddressingMode::ProgramCounterIndirectWithDisplacement:
			ea.value.l = program_counter_.l + int16_t(read_pc<uint16_t>());
			ea.requires_fetch = true;
		break;
		case AddressingMode::ProgramCounterIndirectWithIndex8bitDisplacement:
			ea.value.l = program_counter_.l + index_8bitdisplacement();
			ea.requires_fetch = true;
		break;

		default:
			// TODO.
			assert(false);
		break;
	}

	return ea;
}

template <Model model, typename BusHandler>
void Executor<model, BusHandler>::signal_bus_error(FunctionCode code, uint32_t address) {
	throw AccessException(code, address, Exception::AccessFault);
}

template <Model model, typename BusHandler>
void Executor<model, BusHandler>::set_interrupt_level(int level) {
	interrupt_input_ = level;
}

template <Model model, typename BusHandler>
void Executor<model, BusHandler>::run_for_instructions(int count) {
	while(count > 0) {
		try {
			run(count);
		} catch (uint64_t exception) {
			// Unpack the exception; this is the converse of the AccessException macro.
			const int vector_address = (exception >> 6) & 0xfc;
			const uint16_t code = uint16_t(exception & 0xff);
			const uint32_t faulting_address = uint32_t(exception >> 16);

			// Grab the status to store, then switch into supervisor mode.
			const uint16_t status = status_.status();
			status_.is_supervisor = true;
			status_.trace_flag = 0;
			did_update_status();

			// Push status and the program counter at instruction start.
			write<uint16_t>(sp.l - 14, code);
			write<uint32_t>(sp.l - 12, faulting_address);
			write<uint16_t>(sp.l - 8, instruction_opcode_);
			write<uint16_t>(sp.l - 6, status);
			write<uint16_t>(sp.l - 4, instruction_address_);
			sp.l -= 14;

			// Fetch the new program counter; reset on a double fault.
			try {
				program_counter_.l = read<uint32_t>(vector_address);
			} catch (uint64_t) {
				// TODO: I think this is incorrect, but need to verify consistency
				// across different 680x0s.
				reset_processor();
			}
		}
	}
}

template <Model model, typename BusHandler>
void Executor<model, BusHandler>::run(int &count) {
	while(count--) {
		// Check for a new interrupt.
		if(interrupt_input_ > status_.interrupt_level) {
			const int vector = bus_handler_.acknowlege_interrupt(interrupt_input_);
			if(vector >= 0) {
				raise_exception<false>(vector);
			} else {
				raise_exception<false>(Exception::InterruptAutovectorBase - 1 + interrupt_input_);
			}
			status_.interrupt_level = interrupt_input_;
		}

		// Capture the trace bit, indicating whether to trace
		// after this instruction.
		const auto should_trace = status_.trace_flag;

		// Read the next instruction.
		instruction_address_ = program_counter_.l;
		instruction_opcode_ = read_pc<uint16_t>();
		const Preinstruction instruction = decoder_.decode(instruction_opcode_);

		if(instruction.requires_supervisor() && !status_.is_supervisor) {
			raise_exception(Exception::PrivilegeViolation);
			continue;
		}
		if(instruction.operation == Operation::Undefined) {
			switch(instruction_opcode_ & 0xf000) {
				default:
					raise_exception(Exception::IllegalInstruction);
				continue;
				case 0xa000:
					raise_exception(Exception::Line1010);
				continue;
				case 0xf000:
					raise_exception(Exception::Line1111);
				continue;
			}
		}

		// Temporary storage.
		CPU::SlicedInt32 operand_[2];
		EffectiveAddress effective_address_[2];

		// Calculate effective addresses; copy 'addresses' into the
		// operands by default both: (i) because they might be values,
		// rather than addresses; and (ii) then they'll be there for use
		// by LEA and PEA.
		//
		// TODO: much of this work should be performed by a full Decoder,
		// so that it can be cached.
		effective_address_[0] = calculate_effective_address(instruction, instruction_opcode_, 0);
		effective_address_[1] = calculate_effective_address(instruction, instruction_opcode_, 1);
		operand_[0] = effective_address_[0].value;
		operand_[1] = effective_address_[1].value;

		// Obtain the appropriate sequence.
		//
		// TODO: make a decision about whether this goes into a fully-decoded Instruction.
		const auto flags = operand_flags<model>(instruction.operation);

// TODO: potential alignment exception, here and in store.
#define fetch_operand(n)																\
	if(effective_address_[n].requires_fetch) {											\
		read(instruction.operand_size(), effective_address_[n].value.l, operand_[n]);	\
	}

		if(flags & FetchOp1) {	fetch_operand(0);	}
		if(flags & FetchOp2) {	fetch_operand(1);	}

#undef fetch_operand

		perform<model>(instruction, operand_[0], operand_[1], status_, *this);

// TODO: rephrase to avoid conditional below.
#define store_operand(n)																\
	if(!effective_address_[n].requires_fetch) {											\
		if(instruction.mode(n) == AddressingMode::DataRegisterDirect) {					\
			Dn(instruction.reg(n)) = operand_[n];										\
		} else {																		\
			An(instruction.reg(n)) = operand_[n];										\
		}																				\
	} else {																			\
		write(instruction.operand_size(), effective_address_[n].value.l, operand_[n]);	\
	}

		if(flags & StoreOp1) {	store_operand(0);	}
		if(flags & StoreOp2) {	store_operand(1);	}

#undef store_operand

		// If the trace bit was set, trigger the trace exception.
		if(should_trace) {
			raise_exception<false>(Exception::Trace);
		}
	}
}

// MARK: - State

template <Model model, typename BusHandler>
typename Executor<model, BusHandler>::Registers Executor<model, BusHandler>::get_state() {
	Registers result;

	for(int c = 0; c < 8; c++) {
		result.data[c] = Dn(c).l;
	}
	for(int c = 0; c < 7; c++) {
		result.address[c] = An(c).l;
	}
	result.status = status_.status();
	result.program_counter = program_counter_.l;

	stack_pointers_[active_stack_pointer_] = sp;
	result.user_stack_pointer = stack_pointers_[0].l;
	result.supervisor_stack_pointer = stack_pointers_[1].l;

	return result;
}

template <Model model, typename BusHandler>
void Executor<model, BusHandler>::set_state(const Registers &state) {
	for(int c = 0; c < 8; c++) {
		Dn(c).l = state.data[c];
	}
	for(int c = 0; c < 7; c++) {
		An(c).l = state.address[c];
	}
	status_.set_status(state.status);
	did_update_status();
	program_counter_.l = state.program_counter;

	stack_pointers_[0].l = state.user_stack_pointer;
	stack_pointers_[1].l = state.supervisor_stack_pointer;
	sp = stack_pointers_[active_stack_pointer_];
}

// MARK: - Flow Control.

template <Model model, typename BusHandler>
template <bool use_current_instruction_pc>
void Executor<model, BusHandler>::raise_exception(int index) {
	const uint32_t address = index << 2;

	// Grab the status to store, then switch into supervisor mode
	// and disable tracing.
	const uint16_t status = status_.status();
	status_.is_supervisor = true;
	status_.trace_flag = 0;
	did_update_status();

	// Push status and the program counter at instruction start.
	write<uint32_t>(sp.l - 4, use_current_instruction_pc ? instruction_address_ : program_counter_.l);
	write<uint16_t>(sp.l - 6, status);
	sp.l -= 6;

	// Fetch the new program counter.
	program_counter_.l = read<uint32_t>(address);
}

template <Model model, typename BusHandler>
void Executor<model, BusHandler>::did_update_status() {
	// Shuffle the stack pointers.
	stack_pointers_[active_stack_pointer_] = sp;
	sp = stack_pointers_[int(status_.is_supervisor)];
	active_stack_pointer_ = int(status_.is_supervisor);
}

template <Model model, typename BusHandler>
void Executor<model, BusHandler>::stop() {}

template <Model model, typename BusHandler>
void Executor<model, BusHandler>::reset() {
	bus_handler_.reset();
}

template <Model model, typename BusHandler>
void Executor<model, BusHandler>::jmp(uint32_t address) {
	program_counter_.l = address;
}

template <Model model, typename BusHandler>
template <typename IntT> void Executor<model, BusHandler>::complete_bcc(bool branch, IntT offset) {
	if(branch) {
		program_counter_.l = instruction_address_ + offset + 2;
	}
}

template <Model model, typename BusHandler>
void Executor<model, BusHandler>::complete_dbcc(bool matched_condition, bool overflowed, int16_t offset) {
	if(!matched_condition && !overflowed) {
		program_counter_.l = instruction_address_ + offset + 2;
	}
}

template <Model model, typename BusHandler>
void Executor<model, BusHandler>::bsr(uint32_t offset) {
	sp.l -= 4;
	write<uint32_t>(sp.l, program_counter_.l);
	program_counter_.l = instruction_address_ + offset;
}

template <Model model, typename BusHandler>
void Executor<model, BusHandler>::jsr(uint32_t address) {
	sp.l -= 4;
	write<uint32_t>(sp.l, program_counter_.l);
	program_counter_.l = address;
}

template <Model model, typename BusHandler>
void Executor<model, BusHandler>::link(Preinstruction instruction, uint32_t offset) {
	const auto reg = 8 + instruction.reg<0>();

	sp.l -= 4;
	write<uint32_t>(sp.l, Dn(reg).l);
	Dn(reg) = sp;
	sp.l += offset;
}

template <Model model, typename BusHandler>
void Executor<model, BusHandler>::unlink(uint32_t &address) {
	sp.l = address;
	address = read<uint32_t>(sp.l);
	sp.l += 4;
}

template <Model model, typename BusHandler>
void Executor<model, BusHandler>::pea(uint32_t address) {
	sp.l -= 4;
	write<uint32_t>(sp.l, address);
}

template <Model model, typename BusHandler>
void Executor<model, BusHandler>::rtr() {
	status_.set_ccr(read<uint16_t>(sp.l));
	sp.l += 2;
	rts();
}

template <Model model, typename BusHandler>
void Executor<model, BusHandler>::rte() {
	status_.set_status(read<uint16_t>(sp.l));
	sp.l += 2;
	rts();
}

template <Model model, typename BusHandler>
void Executor<model, BusHandler>::rts() {
	program_counter_.l = read<uint32_t>(sp.l);
	sp.l += 4;
}

template <Model model, typename BusHandler>
void Executor<model, BusHandler>::tas(Preinstruction instruction, uint32_t address) {
	uint8_t value;
	if(instruction.mode<0>() != AddressingMode::DataRegisterDirect) {
		value = read<uint8_t>(address);
		write<uint8_t>(address, value | 0x80);
	} else {
		value = uint8_t(address);
		Dn(instruction.reg<0>()).b = uint8_t(address | 0x80);
	}

	status_.overflow_flag = status_.carry_flag = 0;
	status_.zero_result = value;
	status_.negative_flag = value & 0x80;
}

template <Model model, typename BusHandler>
void Executor<model, BusHandler>::move_to_usp(uint32_t address) {
	stack_pointers_[0].l = address;
}

template <Model model, typename BusHandler>
void Executor<model, BusHandler>::move_from_usp(uint32_t &address) {
	address = stack_pointers_[0].l;
}

template <Model model, typename BusHandler>
template <typename IntT>
void Executor<model, BusHandler>::movep(Preinstruction instruction, uint32_t source, uint32_t dest) {
	if(instruction.mode<0>() == AddressingMode::DataRegisterDirect) {
		// Move register to memory.
		const uint32_t reg = source;
		uint32_t address = dest;

		if constexpr (sizeof(IntT) == 4) {
			write<uint8_t>(address, uint8_t(reg >> 24));
			address += 2;

			write<uint8_t>(address, uint8_t(reg >> 16));
			address += 2;
		}

		write<uint8_t>(address, uint8_t(reg >> 8));
		address += 2;

		write<uint8_t>(address, uint8_t(reg));
	} else {
		// Move memory to register.
		uint32_t &reg = Dn(instruction.reg<1>()).l;
		uint32_t address = source;

		if constexpr (sizeof(IntT) == 4) {
			reg = read<uint8_t>(address) << 24;
			address += 2;

			reg |= read<uint8_t>(address) << 16;
			address += 2;
		} else {
			reg &= 0xffff0000;
		}

		reg |= read<uint8_t>(address) << 8;
		address += 2;

		reg |= read<uint8_t>(address);
	}
}

template <Model model, typename BusHandler>
template <typename IntT>
void Executor<model, BusHandler>::movem_toM(Preinstruction instruction, uint32_t source, uint32_t dest) {
	// Move registers to memory. This is the only permitted use of the predecrement mode,
	// which reverses output order.

	if(instruction.mode<1>() == AddressingMode::AddressRegisterIndirectWithPredecrement) {
		// The structure of the code in the mainline part of the executor is such
		// that the address register will already have been predecremented before
		// reaching here, and it'll have been by two bytes per the operand size
		// rather than according to the instruction size. That's not wanted, so undo it.
		//
		// TODO: with the caveat that the 68020+ have different behaviour:
		//
		// "For the MC68020, MC68030, MC68040, and CPU32, if the addressing register is also
		// moved to memory, the value written is the initial register value decremented by the
		// size of the operation. The MC68000 and MC68010 write the initial register value
		// (not decremented)."
		An(instruction.reg<1>()).l += 2;

		uint32_t address = An(instruction.reg<1>()).l;
		int index = 15;

		while(source) {
			if(source & 1) {
				address -= sizeof(IntT);
				write<IntT>(address, IntT(registers_[index].l));
			}
			--index;
			source >>= 1;
		}

		An(instruction.reg<1>()).l = address;
		return;
	}

	int index = 0;
	while(source) {
		if(source & 1) {
			write<IntT>(dest, IntT(registers_[index].l));
			dest += sizeof(IntT);
		}
		++index;
		source >>= 1;
	}
}

template <Model model, typename BusHandler>
template <typename IntT>
void Executor<model, BusHandler>::movem_toR(Preinstruction instruction, uint32_t source, uint32_t dest) {
	// Move memory to registers.
	//
	// A 68000 convention has been broken here; the instruction form is:
	//	MOVEM <ea>, #
	// ... but the instruction is encoded as [MOVEM] [#] [ea].
	//
	// This project's decoder decodes as #, <ea>.
	int index = 0;
	while(source) {
		if(source & 1) {
			if constexpr (sizeof(IntT) == 2) {
				registers_[index].l = int16_t(read<uint16_t>(dest));
			} else {
				registers_[index].l = read<uint32_t>(dest);
			}
			dest += sizeof(IntT);
		}
		++index;
		source >>= 1;
	}

	if(instruction.mode<1>() == AddressingMode::AddressRegisterIndirectWithPostincrement) {
		// "If the effective address is specified by the postincrement mode ...
		// [i]f the addressing register is also loaded from memory, the memory value is
		// ignored and the register is written with the postincremented effective address."

		An(instruction.reg<1>()).l = dest;
	}
}

#undef sp
#undef Dn
#undef An
#undef AccessException

}
}

#endif /* InstructionSets_M68k_ExecutorImplementation_hpp */
