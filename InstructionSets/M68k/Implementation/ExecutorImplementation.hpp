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
#include <cassert>

namespace InstructionSet {
namespace M68k {

template <Model model, typename BusHandler>
Executor<model, BusHandler>::Executor(BusHandler &handler) : bus_handler_(handler) {
	reset();
}

template <Model model, typename BusHandler>
void Executor<model, BusHandler>::reset() {
	// Establish: supervisor state, all interrupts blocked.
	status_.set_status(0b0010'0011'1000'0000);
	did_update_status();

	// Seed stack pointer and program counter.
	data_[7].l = bus_handler_.template read<uint32_t>(0);
	program_counter_.l = bus_handler_.template read<uint32_t>(4);
}

template <Model model, typename BusHandler>
void Executor<model, BusHandler>::read(DataSize size, uint32_t address, CPU::SlicedInt32 &value) {
	switch(size) {
		case DataSize::Byte:
			value.b = bus_handler_.template read<uint8_t>(address);
		break;
		case DataSize::Word:
			value.w = bus_handler_.template read<uint16_t>(address);
		break;
		case DataSize::LongWord:
			value.l = bus_handler_.template read<uint32_t>(address);
		break;
	}
}

template <Model model, typename BusHandler>
void Executor<model, BusHandler>::write(DataSize size, uint32_t address, CPU::SlicedInt32 value) {
	switch(size) {
		case DataSize::Byte:
			 bus_handler_.template write<uint8_t>(address, value.b);
		break;
		case DataSize::Word:
			bus_handler_.template write<uint16_t>(address, value.w);
		break;
		case DataSize::LongWord:
			bus_handler_.template write<uint32_t>(address, value.l);
		break;
	}
}

template <Model model, typename BusHandler>
template <typename IntT> IntT Executor<model, BusHandler>::read_pc() {
	const IntT result = bus_handler_.template read<IntT>(program_counter_.l);

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
	const uint32_t displacement = (extension & 0x8000) ? address_[register_index].l : data_[register_index].l;
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
			ea.value = data_[instruction.reg(index)];
			ea.requires_fetch = false;
		break;
		case AddressingMode::AddressRegisterDirect:
			ea.value = address_[instruction.reg(index)];
			ea.requires_fetch = false;
		break;
		case AddressingMode::Quick:
			ea.value.l = quick(opcode, instruction.operation);
			ea.requires_fetch = false;
		break;
		case AddressingMode::ImmediateData:
			switch(instruction.size()) {
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
			ea.value = address_[instruction.reg(index)];
			ea.requires_fetch = true;
		break;
		case AddressingMode::AddressRegisterIndirectWithPostincrement: {
			const auto reg = instruction.reg(index);

			ea.value = address_[reg];
			ea.requires_fetch = true;

			switch(instruction.size()) {
				case DataSize::Byte:		address_[reg].l += byte_increments[reg];	break;
				case DataSize::Word:		address_[reg].l += 2;						break;
				case DataSize::LongWord:	address_[reg].l += 4;						break;
			}
		} break;
		case AddressingMode::AddressRegisterIndirectWithPredecrement: {
			const auto reg = instruction.reg(index);

			switch(instruction.size()) {
				case DataSize::Byte:		address_[reg].l -= byte_increments[reg];	break;
				case DataSize::Word:		address_[reg].l -= 2;						break;
				case DataSize::LongWord:	address_[reg].l -= 4;						break;
			}

			ea.value = address_[reg];
			ea.requires_fetch = true;
		} break;
		case AddressingMode::AddressRegisterIndirectWithDisplacement:
			ea.value.l = address_[instruction.reg(index)].l + int16_t(read_pc<uint16_t>());
			ea.requires_fetch = true;
		break;
		case AddressingMode::AddressRegisterIndirectWithIndex8bitDisplacement:
			ea.value.l = address_[instruction.reg(index)].l + index_8bitdisplacement();
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
void Executor<model, BusHandler>::run_for_instructions(int count) {
	while(count--) {
		// TODO: check interrupt level, trace flag.

		// Read the next instruction.
		instruction_address_ = program_counter_.l;
		const auto opcode = read_pc<uint16_t>();
		const Preinstruction instruction = decoder_.decode(opcode);

		if(!status_.is_supervisor_ && instruction.requires_supervisor()) {
			raise_exception(8);
			continue;
		}
		if(instruction.operation == Operation::Undefined) {
			switch(opcode & 0xf000) {
				default:
					raise_exception(4);
				continue;
				case 0xa000:
					raise_exception(10);
				continue;
				case 0xf000:
					raise_exception(11);
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
		effective_address_[0] = calculate_effective_address(instruction, opcode, 0);
		effective_address_[1] = calculate_effective_address(instruction, opcode, 1);
		operand_[0] = effective_address_[0].value;
		operand_[1] = effective_address_[1].value;

		// Obtain the appropriate sequence.
		//
		// TODO: make a decision about whether this goes into a fully-decoded Instruction.
		const auto flags = operand_flags<model>(instruction.operation);

// TODO: potential alignment exception, here and in store.
#define fetch_operand(n)														\
	if(effective_address_[n].requires_fetch) {									\
		read(instruction.size(), effective_address_[n].value.l, operand_[n]);	\
	}

		if(flags & FetchOp1) {	fetch_operand(0);	}
		if(flags & FetchOp2) {	fetch_operand(1);	}

#undef fetch_operand

		perform<model>(instruction, operand_[0], operand_[1], status_, *this);

// TODO: is it worth holding registers as a single block to avoid conditional below?
#define store_operand(n)		\
	if(!effective_address_[n].requires_fetch) {								\
		if(instruction.mode(n) == AddressingMode::DataRegisterDirect) {	\
			data_[instruction.reg(n)] = operand_[n];				\
		} else {															\
			address_[instruction.reg(n)] = operand_[n];				\
		}																	\
	} else {	\
		write(instruction.size(), effective_address_[n].value.l, operand_[n]);	\
	}

		if(flags & StoreOp1) {	store_operand(0);	}
		if(flags & StoreOp2) {	store_operand(1);	}

#undef store_operand
	}
}

// MARK: - State

template <Model model, typename BusHandler>
typename Executor<model, BusHandler>::Registers Executor<model, BusHandler>::get_state() {
	Registers result;

	for(int c = 0; c < 8; c++) {
		result.data[c] = data_[c].l;
	}
	for(int c = 0; c < 7; c++) {
		result.address[c] = address_[c].l;
	}
	result.status = status_.status();
	result.program_counter = program_counter_.l;

	stack_pointers_[status_.is_supervisor_] = address_[7];
	result.user_stack_pointer = stack_pointers_[0].l;
	result.supervisor_stack_pointer = stack_pointers_[1].l;

	return result;
}

template <Model model, typename BusHandler>
void Executor<model, BusHandler>::set_state(const Registers &state) {
	for(int c = 0; c < 8; c++) {
		data_[c].l = state.data[c];
	}
	for(int c = 0; c < 7; c++) {
		address_[c].l = state.address[c];
	}
	status_.set_status(state.status);
	program_counter_.l = state.program_counter;

	stack_pointers_[0].l = state.user_stack_pointer;
	stack_pointers_[1].l = state.supervisor_stack_pointer;
	address_[7] = stack_pointers_[status_.is_supervisor_];
}

// MARK: - Flow Control.
// TODO: flow control, all below here.

template <Model model, typename BusHandler>
void Executor<model, BusHandler>::raise_exception(int index) {
	const uint32_t address = index << 2;

	// Grab the status to store, then switch into supervisor mode.
	const uint16_t status = status_.status();
	status_.is_supervisor_ = 1;
	did_update_status();

	// Push status and the program counter at instruction start.
	bus_handler_.template write<uint32_t>(address_[7].l - 4, instruction_address_);
	bus_handler_.template write<uint16_t>(address_[7].l - 6, status);
	address_[7].l -= 6;

	// Fetch the new program counter.
	program_counter_.l = bus_handler_.template read<uint32_t>(address);
}

template <Model model, typename BusHandler>
void Executor<model, BusHandler>::did_update_status() {
	// Shuffle the stack pointers.
	stack_pointers_[active_stack_pointer_] = address_[7];
	address_[7] = stack_pointers_[status_.is_supervisor_];
	active_stack_pointer_ = status_.is_supervisor_;
}

template <Model model, typename BusHandler>
void Executor<model, BusHandler>::stop() {}

template <Model model, typename BusHandler>
void Executor<model, BusHandler>::set_pc(uint32_t) {}

template <Model model, typename BusHandler>
void Executor<model, BusHandler>::add_pc(uint32_t) {}

}
}

#endif /* InstructionSets_M68k_ExecutorImplementation_hpp */
