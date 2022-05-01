//
//  Executor.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 29/04/2022.
//  Copyright © 2022 Thomas Harte. All rights reserved.
//

#include "Executor.hpp"

#include "Perform.hpp"

#include <cassert>

using namespace InstructionSet::M68k;

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
			ea.is_address = false;
		break;
		case AddressingMode::AddressRegisterDirect:
			ea.value = address_[instruction.reg(index)];
			ea.is_address = false;
		break;
		case AddressingMode::Quick:
			ea.value = quick(instruction.operation, opcode);
			ea.is_address = false;
		break;

		//
		// Operands that are effective addresses.
		//

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
		const auto instruction_address = program_counter_.l;
		const uint16_t opcode = bus_handler_.template read<uint16_t>(program_counter_.l);
		const Preinstruction instruction = decoder_.decode(opcode);
		program_counter_.l += 2;

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
		operand_[0].l = effective_address_[0].value;
		operand_[1].l = effective_address_[1].value;

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
					if(!effective_address_[index].is_address) continue;

					// TODO: potential bus alignment exception.

					switch(instruction.size()) {
						case DataSize::Byte:
							operand_[index].l = bus_handler_.template read<uint8_t>(effective_address_[index].value);
						break;
						case DataSize::Word:
							operand_[index].l = bus_handler_.template read<uint16_t>(effective_address_[index].value);
						break;
						case DataSize::LongWord:
							operand_[index].l = bus_handler_.template read<uint32_t>(effective_address_[index].value);
						break;
					}
				} break;

				case Step::Perform:
					perform<model>(instruction, operand_[0], operand_[1], status_, this);
				break;

				case Step::StoreOp1:
				case Step::StoreOp2: {
					const auto index = int(step) & 1;

					// If the operand wasn't indirect, it's already fetched.
					if(!effective_address_[index].is_address) {
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

					switch(instruction.size()) {
						case DataSize::Byte:
							 bus_handler_.write(effective_address_[index].value, operand_[index].b);
						break;
						case DataSize::Word:
							bus_handler_.write(effective_address_[index].value, operand_[index].w);
						break;
						case DataSize::LongWord:
							bus_handler_.write(effective_address_[index].value, operand_[index].l);
						break;
					}
				} break;
			}
		}
	}
}
