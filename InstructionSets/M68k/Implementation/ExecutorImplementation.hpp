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

#define sp()	registers_[8 + 7]

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
	sp().l = bus_handler_.template read<uint32_t>(0);
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
			ea.value = registers_[instruction.reg(index)];
			ea.requires_fetch = false;
		break;
		case AddressingMode::AddressRegisterDirect:
			ea.value = registers_[8 + instruction.reg(index)];
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
			ea.value = registers_[8 + instruction.reg(index)];
			ea.requires_fetch = true;
		break;
		case AddressingMode::AddressRegisterIndirectWithPostincrement: {
			const auto reg = instruction.reg(index);

			ea.value = registers_[8 + reg];
			ea.requires_fetch = true;

			switch(instruction.size()) {
				case DataSize::Byte:		registers_[8 + reg].l += byte_increments[reg];	break;
				case DataSize::Word:		registers_[8 + reg].l += 2;						break;
				case DataSize::LongWord:	registers_[8 + reg].l += 4;						break;
			}
		} break;
		case AddressingMode::AddressRegisterIndirectWithPredecrement: {
			const auto reg = instruction.reg(index);

			switch(instruction.size()) {
				case DataSize::Byte:		registers_[8 + reg].l -= byte_increments[reg];	break;
				case DataSize::Word:		registers_[8 + reg].l -= 2;						break;
				case DataSize::LongWord:	registers_[8 + reg].l -= 4;						break;
			}

			ea.value = registers_[8 + reg];
			ea.requires_fetch = true;
		} break;
		case AddressingMode::AddressRegisterIndirectWithDisplacement:
			ea.value.l = registers_[8 + instruction.reg(index)].l + int16_t(read_pc<uint16_t>());
			ea.requires_fetch = true;
		break;
		case AddressingMode::AddressRegisterIndirectWithIndex8bitDisplacement:
			ea.value.l = registers_[8 + instruction.reg(index)].l + index_8bitdisplacement();
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

// TODO: rephrase to avoid conditional below.
#define store_operand(n)		\
	if(!effective_address_[n].requires_fetch) {								\
		if(instruction.mode(n) == AddressingMode::DataRegisterDirect) {	\
			registers_[instruction.reg(n)] = operand_[n];				\
		} else {															\
			registers_[8 + instruction.reg(n)] = operand_[n];				\
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
		result.data[c] = registers_[c].l;
	}
	for(int c = 0; c < 7; c++) {
		result.address[c] = registers_[8 + c].l;
	}
	result.status = status_.status();
	result.program_counter = program_counter_.l;

	stack_pointers_[status_.is_supervisor_] = sp();
	result.user_stack_pointer = stack_pointers_[0].l;
	result.supervisor_stack_pointer = stack_pointers_[1].l;

	return result;
}

template <Model model, typename BusHandler>
void Executor<model, BusHandler>::set_state(const Registers &state) {
	for(int c = 0; c < 8; c++) {
		registers_[c].l = state.data[c];
	}
	for(int c = 0; c < 7; c++) {
		registers_[8 + c].l = state.address[c];
	}
	status_.set_status(state.status);
	program_counter_.l = state.program_counter;

	stack_pointers_[0].l = state.user_stack_pointer;
	stack_pointers_[1].l = state.supervisor_stack_pointer;
	sp() = stack_pointers_[status_.is_supervisor_];
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
	bus_handler_.template write<uint32_t>(sp().l - 4, instruction_address_);
	bus_handler_.template write<uint16_t>(sp().l - 6, status);
	sp().l -= 6;

	// Fetch the new program counter.
	program_counter_.l = bus_handler_.template read<uint32_t>(address);
}

template <Model model, typename BusHandler>
void Executor<model, BusHandler>::did_update_status() {
	// Shuffle the stack pointers.
	stack_pointers_[active_stack_pointer_] = sp();
	sp() = stack_pointers_[status_.is_supervisor_];
	active_stack_pointer_ = status_.is_supervisor_;
}

template <Model model, typename BusHandler>
void Executor<model, BusHandler>::stop() {}

template <Model model, typename BusHandler>
void Executor<model, BusHandler>::set_pc(uint32_t address) {
	program_counter_.l = address;
}

template <Model model, typename BusHandler>
void Executor<model, BusHandler>::add_pc(uint32_t offset) {
	program_counter_.l = instruction_address_ + offset;
}

template <Model model, typename BusHandler>
void Executor<model, BusHandler>::bsr(uint32_t offset) {
	sp().l -= 4;
	bus_handler_.template write<uint32_t>(sp().l, program_counter_.l);
	program_counter_.l = instruction_address_ + offset;
}

template <Model model, typename BusHandler>
void Executor<model, BusHandler>::jsr(uint32_t address) {
	sp().l -= 4;
	bus_handler_.template write<uint32_t>(sp().l, program_counter_.l);
	program_counter_.l = address;
}

template <Model model, typename BusHandler>
void Executor<model, BusHandler>::link(uint32_t &address, uint32_t offset) {
	sp().l -= 4;
	bus_handler_.template write<uint32_t>(sp().l, address);
	address = sp().l;
	sp().l += offset;
}

template <Model model, typename BusHandler>
void Executor<model, BusHandler>::unlink(uint32_t &address) {
	sp().l = address;
	address = bus_handler_.template read<uint32_t>(sp().l);
	sp().l += 4;
}

template <Model model, typename BusHandler>
template <typename IntT>
void Executor<model, BusHandler>::movep(Preinstruction instruction, uint32_t source, uint32_t dest) {
	if(instruction.mode<0>() == AddressingMode::DataRegisterDirect) {
		// Move register to memory.
		const uint32_t reg = source;
		uint32_t address = dest;

		if constexpr (sizeof(IntT) == 4) {
			bus_handler_.template write<uint8_t>(address, uint8_t(reg >> 24));
			address += 2;

			bus_handler_.template write<uint8_t>(address, uint8_t(reg >> 16));
			address += 2;
		}

		bus_handler_.template write<uint8_t>(address, uint8_t(reg >> 8));
		address += 2;

		bus_handler_.template write<uint8_t>(address, uint8_t(reg));
	} else {
		// Move memory to register.
		uint32_t &reg = registers_[instruction.reg<1>()].l;
		uint32_t address = source;

		if constexpr (sizeof(IntT) == 4) {
			reg = bus_handler_.template read<uint8_t>(address) << 24;
			address += 2;

			reg |= bus_handler_.template read<uint8_t>(address) << 16;
			address += 2;
		} else {
			reg &= 0xffff0000;
		}

		reg |= bus_handler_.template read<uint8_t>(address) << 8;
		address += 2;

		reg |= bus_handler_.template read<uint8_t>(address);
	}
}

template <Model model, typename BusHandler>
template <typename IntT>
void Executor<model, BusHandler>::movem(Preinstruction instruction, uint32_t source, uint32_t dest) {
	if(instruction.mode<0>() == AddressingMode::ImmediateData) {
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
			registers_[8 + instruction.reg<1>()].l += 2;

			uint32_t reg = registers_[8 + instruction.reg<1>()].l;
			int index = 15;

			while(source) {
				if(source & 1) {
					reg -= sizeof(IntT);
					bus_handler_.template write<IntT>(reg, IntT(registers_[index].l));
				}
				--index;
				source >>= 1;
			}

			registers_[8 + instruction.reg<1>()].l = reg;
			return;
		}

		int index = 0;
		while(source) {
			if(source & 1) {
				bus_handler_.template write<IntT>(dest, IntT(registers_[index].l));
				dest += sizeof(IntT);
			}
			++index;
			source >>= 1;
		}

	} else {
		// Move memory to registers.
		int index = 0;
		while(dest) {
			if(dest & 1) {
				if constexpr (sizeof(IntT) == 2) {
					registers_[index].l = int16_t(bus_handler_.template read<uint16_t>(source));
				} else {
					registers_[index].l = bus_handler_.template read<uint32_t>(source);
				}
				source += sizeof(IntT);
			}
			++index;
			dest >>= 1;
		}

		if(instruction.mode<0>() == AddressingMode::AddressRegisterIndirectWithPostincrement) {
			// If the effective address is specified by the postincrement mode ...
			// [i]f the addressing register is also loaded from memory, the memory value is
			// ignored and the register is written with the postincremented effective address.

			registers_[8 + instruction.reg<0>()].l = source;
		}
	}
}

#undef sp

}
}

#endif /* InstructionSets_M68k_ExecutorImplementation_hpp */
