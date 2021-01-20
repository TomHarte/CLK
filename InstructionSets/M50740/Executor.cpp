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
	set_program_counter(uint16_t(memory_[0x1ffe] | (memory_[0x1fff] << 8)));
}

template <Operation operation, AddressingMode addressing_mode> void Executor::perform() {
	// Deal with all modes that don't access memory up here;
	// those that access memory will go through a slightly longer
	// sequence below that wraps the address and checks whether
	// a write is valid [if required].

	int address;
#define next8()		memory_[(program_counter_ + 1) & 0x1fff]
#define next16()	(memory_[(program_counter_ + 1) & 0x1fff] | (memory_[(program_counter_ + 2) & 0x1fff] << 8))

	printf("%d %d\n", int(operation), int(addressing_mode));

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

//			case AddressingMode::Relative:
				// These are all the branches...
//				address = program_counter_ + size(addressing_mode) + int8_t(next8());
//			return;

//			case AddressingMode::ImmediateZeroPage:
				// LDM only...
//			return;

//			case AddressingMode::SpecialPage:	address = 0x1f00 | next8();			break;
				// JSR only...

			/* TODO:

					AccumulatorRelative
					ZeroPageRelative

					... which are BBC/BBS-exclusive.
			*/

		// Addressing modes with a memory access.

			case AddressingMode::Absolute:		address = next16();					break;
			case AddressingMode::AbsoluteX:		address = next16() + x_;			break;
			case AddressingMode::AbsoluteY:		address = next16() + y_;			break;
			case AddressingMode::ZeroPage:		address = next8();					break;
			case AddressingMode::ZeroPageX:		address = (next8() + x_) & 0xff;	break;
			case AddressingMode::ZeroPageY:		address = (next8() + x_) & 0xff;	break;

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

			default:
				assert(false);
	}

#undef next16
#undef next8
	program_counter_ += 1 + size(addressing_mode);

	// Check for a branch; those don't go through the memory accesses below.
	switch(operation) {
		case Operation::JMP:
			set_program_counter(uint16_t(address));
		return;

		case Operation::JSR:
			// TODO: push!
			set_program_counter(uint16_t(address));
		return;

		/* TODO: all other types of branches and calls. */

		default: break;
	}


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
#define set_nz(a)	negative_result_ = zero_result_ = (a)
	switch(operation) {
		case Operation::LDA:	set_nz(a_ = *operand);	break;
		case Operation::LDX:	set_nz(x_ = *operand);	break;
		case Operation::LDY:	set_nz(y_ = *operand);	break;

		case Operation::STA:	*operand = a_;	break;
		case Operation::STX:	*operand = x_;	break;
		case Operation::STY:	*operand = y_;	break;

		case Operation::TXA:	set_nz(a_ = x_);	break;
		case Operation::TYA:	set_nz(a_ = y_);	break;
		case Operation::TXS:	s_ = x_;			break;
		case Operation::TAX:	set_nz(x_ = a_);	break;
		case Operation::TAY:	set_nz(y_ = a_);	break;
		case Operation::TSX:	set_nz(x_ = s_);	break;

		case Operation::SEB0:	case Operation::SEB1:	case Operation::SEB2:	case Operation::SEB3:
		case Operation::SEB4:	case Operation::SEB5:	case Operation::SEB6:	case Operation::SEB7:
			*operand |= 1 << (int(operation) - int(Operation::SEB0));
		break;
		case Operation::CLB0:	case Operation::CLB1:	case Operation::CLB2:	case Operation::CLB3:
		case Operation::CLB4:	case Operation::CLB5:	case Operation::CLB6:	case Operation::CLB7:
			*operand &= ~(1 << (int(operation) - int(Operation::CLB0)));
		break;

		case Operation::CLI:	interrupt_disable_ = 0x00;		break;
		case Operation::SEI:	interrupt_disable_ = 0xff;		break;
		case Operation::CLT:	index_mode_ = false;			break;
		case Operation::SET:	index_mode_ = true;				break;
		case Operation::CLD:	decimal_mode_ = false;			break;
		case Operation::SED:	decimal_mode_ = true;			break;

		default:
			printf("Unimplemented operation: %d\n", int(operation));
			assert(false);
	}
#undef set_nz
}

void Executor::set_program_counter(uint16_t address) {
	program_counter_ = address;
	CachingExecutor::set_program_counter(address);
}
