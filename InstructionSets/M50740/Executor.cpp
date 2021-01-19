//
//  Executor.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 16/1/21.
//  Copyright © 2021 Thomas Harte. All rights reserved.
//

#include "Executor.hpp"

#include <algorithm>
#include <cassert>
#include <cstring>

using namespace InstructionSet::M50740;

Executor::Executor() {
	// Cut down the list of all generated performers to those the processor actually uses, and install that
	// for future referencing by action_for.
	Decoder decoder;
	for(size_t c = 0; c < 256; c++) {
		const auto instruction = decoder.instrucion_for_opcode(uint8_t(c));

		// Treat invalid as NOP, because I've got to do _something_.
		if(instruction.operation == Operation::Invalid) {
			performers_[c] = performer_lookup_.performer(Operation::NOP, instruction.addressing_mode);
		} else {
			performers_[c] = performer_lookup_.performer(instruction.operation, instruction.addressing_mode);
		}
	}
}

void Executor::set_rom(const std::vector<uint8_t> &rom) {
	// Copy into place, and reset.
	const auto length = std::min(size_t(0x1000), rom.size());
	memcpy(&memory_[0x2000 - length], rom.data(), length);
	reset();

	// TEMPORARY: just to test initial wiring.
	perform_all();
}

void Executor::reset() {
	// Just jump to the reset vector.
	set_program_counter(uint16_t(memory_[0x1ffe] | (memory_[0x1fff] << 8)) & 0x1fff);
}

template <Operation operation, AddressingMode addressing_mode> void Executor::perform() {
	// Deal with all modes that don't access memory up here;
	// those that access memory will go through a slightly longer
	// sequence below that wraps the address and checks whether
	// a write is valid [if required].

	int address;
#define next8()		memory_[(program_counter_ + 1) & 0x1fff]
#define next16()	(memory_[(program_counter_ + 1) & 0x1fff] | (memory_[(program_counter_ + 2) & 0x1fff] << 8))

	// Underlying assumption below: the instruction stream will never
	// overlap with IO ports.
	switch(addressing_mode) {

		// Addressing modes with no further memory access.

			case AddressingMode::Implied:
				perform<operation>(nullptr);
				++program_counter_;
			return;

			case AddressingMode::Accumulator:
				perform<operation>(&a_);
				++program_counter_;
			return;

			case AddressingMode::Immediate:
				perform<operation>(&next8());
				program_counter_ += 2;
			return;

			case AddressingMode::Relative:
				// These are all the branches...
//				address = program_counter_ + size(addressing_mode) + int8_t(next8());
			return;

		// Addressing modes with a memory access.

			case AddressingMode::Absolute:		address = next16();					break;
			case AddressingMode::AbsoluteX:		address = next16() + x_;			break;
			case AddressingMode::AbsoluteY:		address = next16() + y_;			break;
			case AddressingMode::ZeroPage:		address = next8();					break;
			case AddressingMode::ZeroPageX:		address = (next8() + x_) & 0xff;	break;
			case AddressingMode::ZeroPageY:		address = (next8() + x_) & 0xff;	break;
			case AddressingMode::SpecialPage:	address = 0x1f00 | next8();			break;

			case AddressingMode::ZeroPageIndirect:
				address = next8();
				address = memory_[address] | (memory_[(address + 1) & 0xff] << 8);
			break;

			case AddressingMode::XIndirect:
				address = (next8() + x_) & 0xff;
				address = memory_[address] | (memory_[(address + 1)&0xff] << 8);
			break;

			case AddressingMode::IndirectY:
				address = (memory_[next8()] | (memory_[(next8()+1)&0xff] << 8)) + y_;
			break;

			case AddressingMode::AbsoluteIndirect:
				address = next16();
				address = memory_[address] | (memory_[(address + 1) & 0x1fff] << 8);
			break;

			/* TODO:

					ImmediateZeroPage (for LDM)
					BitXAccumulator
					BitXZeroPage
					BitXAccumulatorRelative
					BitXZeroPageRelative
			*/


			default:
				assert(false);
	}

#undef next16
#undef next8

	program_counter_ += 1 + size(addressing_mode);
	assert(access_type(operation) != AccessType::None);

	// TODO: full reading/writing logic here; only the first 96 bytes are RAM,
	// there are also timers and IO ports to handle.

	if constexpr(access_type(operation) == AccessType::Read) {
		perform<operation>(&memory_[address & 0x1fff]);
		return;
	}

	uint8_t value = memory_[address & 0x1fff];
	perform<operation>(&value);

	memory_[address & 0x1fff] = value;
}

template <Operation operation> void Executor::perform(uint8_t *operand [[maybe_unused]]) {
	switch(operation) {
		case Operation::LDA:	a_ = *operand;	break;	// TODO: flags (for all three here).
		case Operation::LDX:	x_ = *operand;	break;
		case Operation::LDY:	y_ = *operand;	break;

		case Operation::STA:	*operand = a_;	break;
		case Operation::STX:	*operand = x_;	break;
		case Operation::STY:	*operand = y_;	break;

		default: assert(false);
	}
}

void Executor::set_program_counter(uint16_t address) {
	program_counter_ = address;
	CachingExecutor::set_program_counter(address);
}
