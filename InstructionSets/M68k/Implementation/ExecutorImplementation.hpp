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

	// Seed stack pointer and program counter.
	data_[7] = bus_handler_.template read<uint32_t>(0);
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
	const int register_index = (extension >> 11) & 7;
	const uint32_t displacement = (extension & 0x8000) ? address_[register_index].l : data_[register_index].l;
	return offset + (extension & 0x800) ? displacement : uint16_t(displacement);
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
			ea.value.l = data_[instruction.reg(index)];
			ea.requires_fetch = false;
		break;
		case AddressingMode::AddressRegisterDirect:
			ea.value.l = address_[instruction.reg(index)];
			ea.requires_fetch = false;
		break;
		case AddressingMode::Quick:
			ea.value.l = quick(instruction.operation, opcode);
			ea.requires_fetch = false;
		break;
		case AddressingMode::ImmediateData:
			read(instruction.size(), program_counter_.l, ea.value.l);
			program_counter_.l += (instruction.size() == DataSize::LongWord) ? 4 : 2;
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
			ea.value.l = address_[instruction.reg(index)];
			ea.requires_fetch = true;
		break;
		case AddressingMode::AddressRegisterIndirectWithPostincrement: {
			const auto reg = instruction.reg(index);

			ea.value.l = address_[reg];
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

			ea.value.l = address_[reg];
			ea.requires_fetch = true;
		} break;
		case AddressingMode::AddressRegisterIndirectWithDisplacement:
			ea.value.l = address_[instruction.reg(index)] + int16_t(read_pc<uint16_t>());
			ea.requires_fetch = true;
		break;
		case AddressingMode::AddressRegisterIndirectWithIndex8bitDisplacement:
			ea.value.l = address_[instruction.reg(index)] + index_8bitdisplacement();
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
		program_counter_.l += 2;

		// TODO: check privilege level.

		// Temporary storage.
		CPU::SlicedInt32 operand_[2];
		EffectiveAddress effective_address_[2];

		// Calculate effective addresses; copy 'addresses' into the
		// operands by default both: (i) because they might be values,
		// rather than addresses; and (ii) then they'll be there for use
		// by LEA and PEA.
		//
		// TODO: this work should be performed by a full Decoder, so that it can be cached.
		effective_address_[0] = calculate_effective_address(instruction, opcode, 0);
		effective_address_[1] = calculate_effective_address(instruction, opcode, 1);
		operand_[0] = effective_address_[0].value;
		operand_[1] = effective_address_[1].value;

		// Obtain the appropriate sequence.
		//
		// TODO: make a decision about whether this goes into a fully-decoded Instruction.
		Sequence<model> sequence(instruction.operation);

		// Perform it.
		while(!sequence.empty()) {
			const auto step = sequence.pop_front();

			switch(step) {
				default: assert(false);	// i.e. TODO

				case Step::FetchOp1:
				case Step::FetchOp2: {
					const auto index = int(step) & 1;

					// If the operand wasn't indirect, it's already fetched.
					if(!effective_address_[index].requires_fetch) continue;

					// TODO: potential bus alignment exception.
					read(instruction.size(), effective_address_[index].value, operand_[index]);
				} break;

				case Step::Perform:
					perform<model>(instruction, operand_[0], operand_[1], status_, this);
				break;

				case Step::StoreOp1:
				case Step::StoreOp2: {
					const auto index = int(step) & 1;

					// If the operand wasn't indirect, store directly to Dn or An.
					if(!effective_address_[index].requires_fetch) {
						// This must be either address or data register indirect.
						assert(
							instruction.mode(index) == AddressingMode::DataRegisterDirect ||
							instruction.mode(index) == AddressingMode::AddressRegisterDirect);

						// TODO: is it worth holding registers as a single block to avoid this conditional?
						if(instruction.mode(index) == AddressingMode::DataRegisterDirect) {
							data_[instruction.reg(index)] = operand_[index];
						} else {
							address_[instruction.reg(index)] = operand_[index];
						}

						break;
					}

					// TODO: potential bus alignment exception.
					write(instruction.size(), effective_address_[index].value, operand_[index]);
				} break;
			}
		}
	}
}

}
}

#endif /* InstructionSets_M68k_ExecutorImplementation_hpp */
