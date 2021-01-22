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
	run_for(Cycles(13000));
}

void Executor::run_for(Cycles cycles) {
	CachingExecutor::run_for(cycles.as<int>());
}

void Executor::reset() {
	// Just jump to the reset vector.
	set_program_counter(uint16_t(memory_[0x1ffe] | (memory_[0x1fff] << 8)));
}

uint8_t Executor::read(uint16_t address) {
	address &= 0x1fff;
	switch(address) {
		default: return memory_[address];

		// TODO: external IO ports.

		// "Port R"; sixteen four-bit ports
		case 0xd0: case 0xd1: case 0xd2: case 0xd3: case 0xd4: case 0xd5: case 0xd6: case 0xd7:
		case 0xd8: case 0xd9: case 0xda: case 0xdb: case 0xdc: case 0xdd: case 0xde: case 0xdf:
			printf("TODO: Port R\n");
		return 0xff;

		// Ports P0–P3.
		case 0xe0: case 0xe1:
		case 0xe2: case 0xe3:
		case 0xe4: case 0xe5:
		case 0xe8: case 0xe9:
			printf("TODO: Ports P0–P3\n");
		return 0xff;

		// Timers.
		case 0xf9: case 0xfa: case 0xfb: case 0xfc: case 0xfd: case 0xfe: case 0xff:
			printf("TODO: Timers\n");
		return 0xff;
	}
}

void Executor::write(uint16_t address, uint8_t value) {
	address &= 0x1fff;
	if(address < 0x60) {
		memory_[address] = value;
		return;
	}

	// TODO: all external IO ports.
}

void Executor::push(uint8_t value) {
	write(s_, value);
	--s_;
}

uint8_t Executor::pull() {
	++s_;
	return read(s_);
}

void Executor::set_flags(uint8_t flags) {
	negative_result_ = flags;
	overflow_result_ = uint8_t(flags << 1);
	index_mode_ = flags & 0x20;
	decimal_mode_ = flags & 0x08;
	interrupt_disable_ = flags & 0x04;
	zero_result_ = !(flags & 0x02);
	carry_flag_ = flags & 0x01;
}

uint8_t Executor::flags() {
	return
		(negative_result_ & 0x80) |
		((overflow_result_ & 0x80) >> 1) |
		(index_mode_ ? 0x20 : 0x00) |
		(decimal_mode_ ? 0x08 : 0x00) |
		interrupt_disable_ |
		(zero_result_ ? 0x00 : 0x02) |
		carry_flag_;
}

template<bool is_brk> inline void Executor::perform_interrupt() {
	// BRK has an unused operand.
	++program_counter_;
	push(uint8_t(program_counter_ >> 8));
	push(uint8_t(program_counter_ & 0xff));
	push(flags() | (is_brk ? 0x10 : 0x00));
	set_program_counter(uint16_t(memory_[0x1ff4] | (memory_[0x1ff5] << 8)));
}

template <Operation operation, AddressingMode addressing_mode> void Executor::perform() {
	printf("%04x\t%02x\t%d %d\t[x:%02x s:%02x]\n", program_counter_ & 0x1fff, memory_[program_counter_ & 0x1fff], int(operation), int(addressing_mode), x_, s_);

	// Post cycle cost; this emulation _does not provide accurate timing_.
	// TODO: post actual cycle counts. For now count instructions only.
	subtract_duration(1);

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

		// Special-purpose addressing modes.

			case AddressingMode::Relative:
				address = program_counter_ + 1 + size(addressing_mode) + int8_t(next8());
			break;

			case AddressingMode::SpecialPage:	address = 0x1f00 | next8();			break;

			case AddressingMode::ImmediateZeroPage:
				// LDM only...
				write(memory_[(program_counter_+2)&0x1fff], memory_[(program_counter_+1)&0x1fff]);
				program_counter_ += 1 + size(addressing_mode);
			return;

			case AddressingMode::AccumulatorRelative:
			case AddressingMode::ZeroPageRelative: {
				// Order of bytes is: (i) zero page address; (ii) relative jump.
				uint8_t value;
				if constexpr (addressing_mode == AddressingMode::AccumulatorRelative) {
					value = a_;
					address = program_counter_ + 1 + size(addressing_mode) + int8_t(next8());
				} else {
					value = read(next8());
					address = program_counter_ + 1 + size(addressing_mode) + int8_t(memory_[(program_counter_+2)&0x1fff]);
				}
				program_counter_ += 1 + size(addressing_mode);
				switch(operation) {
					case Operation::BBS0:	case Operation::BBS1:	case Operation::BBS2:	case Operation::BBS3:
					case Operation::BBS4:	case Operation::BBS5:	case Operation::BBS6:	case Operation::BBS7:
						if(value & (1 << (int(operation) - int(Operation::BBS0)))) set_program_counter(uint16_t(address));
					return;
					case Operation::BBC0:	case Operation::BBC1:	case Operation::BBC2:	case Operation::BBC3:
					case Operation::BBC4:	case Operation::BBC5:	case Operation::BBC6:	case Operation::BBC7:
						if(value & (1 << (int(operation) - int(Operation::BBC0)))) set_program_counter(uint16_t(address));
					return;
					default: assert(false);
				}
			} break;

		// Addressing modes with a memory access.

			case AddressingMode::Absolute:		address = next16();					break;
			case AddressingMode::AbsoluteX:		address = next16() + x_;			break;
			case AddressingMode::AbsoluteY:		address = next16() + y_;			break;
			case AddressingMode::ZeroPage:		address = next8();					break;
			case AddressingMode::ZeroPageX:		address = (next8() + x_) & 0xff;	break;
			case AddressingMode::ZeroPageY:		address = (next8() + y_) & 0xff;	break;

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
		case Operation::BRA: case Operation::JMP:
			set_program_counter(uint16_t(address));
		return;

		case Operation::JSR: {
			const auto return_address = program_counter_ - 1;
			push(uint8_t(return_address >> 8));
			push(uint8_t(return_address & 0xff));
			set_program_counter(uint16_t(address));
		} return;

		case Operation::BPL:	if(!(negative_result_&0x80))	set_program_counter(uint16_t(address));	return;
		case Operation::BMI:	if(negative_result_&0x80)		set_program_counter(uint16_t(address));	return;
		case Operation::BEQ:	if(!zero_result_)				set_program_counter(uint16_t(address));	return;
		case Operation::BNE:	if(zero_result_)				set_program_counter(uint16_t(address));	return;
		case Operation::BCS:	if(carry_flag_)					set_program_counter(uint16_t(address));	return;
		case Operation::BCC:	if(!carry_flag_)				set_program_counter(uint16_t(address));	return;
		case Operation::BVS:	if(overflow_result_ & 0x80)		set_program_counter(uint16_t(address));	return;
		case Operation::BVC:	if(!(overflow_result_ & 0x80))	set_program_counter(uint16_t(address));	return;

		default: break;
	}

	assert(access_type(operation) != AccessType::None);

	if constexpr(access_type(operation) == AccessType::Read) {
		uint8_t source = read(uint16_t(address));
		perform<operation>(&source);
		return;
	}

	uint8_t value;
	if constexpr(access_type(operation) == AccessType::ReadModifyWrite) {
		value = read(uint16_t(address));
	} else {
		value = 0xff;
	}
	perform<operation>(&value);
	write(uint16_t(address), value);
}

template <Operation operation> void Executor::perform(uint8_t *operand [[maybe_unused]]) {

#define set_nz(a)	negative_result_ = zero_result_ = (a)
	switch(operation) {
		case Operation::LDA:
			if(index_mode_) {
				write(x_, *operand);
				set_nz(*operand);
			} else {
				set_nz(a_ = *operand);
			}
		break;
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
		case Operation::SEI:	interrupt_disable_ = 0x04;		break;
		case Operation::CLT:	index_mode_ = false;			break;
		case Operation::SET:	index_mode_ = true;				break;
		case Operation::CLD:	decimal_mode_ = false;			break;
		case Operation::SED:	decimal_mode_ = true;			break;
		case Operation::CLC:	carry_flag_ = 0;				break;
		case Operation::SEC:	carry_flag_ = 1;				break;
		case Operation::CLV:	overflow_result_ = 0;			break;

		case Operation::DEX:	--x_; set_nz(x_);				break;
		case Operation::INX:	++x_; set_nz(x_);				break;
		case Operation::DEY:	--y_; set_nz(y_);				break;
		case Operation::INY:	++y_; set_nz(y_);				break;
		case Operation::DEC:	--*operand; set_nz(*operand);	break;
		case Operation::INC:	++*operand; set_nz(*operand);	break;

		case Operation::RTS: {
			uint16_t target = pull();
			target |= pull() << 8;
			set_program_counter(target+1);
			--program_counter_;				// To undo the unavoidable increment
											// after exiting from here.
		} break;

		case Operation::RTI: {
			set_flags(pull());
			uint16_t target = pull();
			target |= pull() << 8;
			set_program_counter(target);
			--program_counter_;				// To undo the unavoidable increment
											// after exiting from here.
		} break;

		case Operation::BRK:
			perform_interrupt<true>();
			--program_counter_;				// To undo the unavoidable increment
											// after exiting from here.
		break;

		case Operation::STP:	set_is_stopped(true);		break;

		case Operation::COM:	set_nz(*operand ^= 0xff);	break;

		case Operation::FST:	case Operation::SLW:	case Operation::NOP:
			// TODO: communicate FST and SLW onwards, I imagine. Find out what they interface with.
		break;

		case Operation::PHA:	push(a_);				break;
		case Operation::PHP:	push(flags());			break;
		case Operation::PLA:	set_nz(a_ = pull());	break;
		case Operation::PLP:	set_flags(pull());		break;

		case Operation::ASL:
			carry_flag_ = *operand >> 7;
			*operand <<= 1;
			set_nz(*operand);
		break;

		case Operation::LSR:
			carry_flag_ = *operand & 1;
			*operand >>= 1;
			set_nz(*operand);
		break;

		case Operation::ROL: {
			const uint8_t temp8 = uint8_t((*operand << 1) | carry_flag_);
			carry_flag_ = *operand >> 7;
			set_nz(*operand = temp8);
		} break;

		case Operation::ROR: {
			const uint8_t temp8 = uint8_t((*operand >> 1) | (carry_flag_ << 7));
			carry_flag_ = *operand & 1;
			set_nz(*operand = temp8);
		} break;

		case Operation::RRF:
			*operand = uint8_t((*operand >> 4) | (*operand << 4));
		break;

		case Operation::BIT:
			zero_result_ = *operand & a_;
			negative_result_ = *operand;
			overflow_result_ = uint8_t(*operand << 1);
		break;

		case Operation::TST:
			set_nz(*operand);
		break;

	/*
		Operations affected by the index mode flag: ADC, AND, CMP, EOR, LDA, ORA, and SBC.
	*/

#define index(op)					\
		if(index_mode_) {			\
			uint8_t t = read(x_);	\
			op(t);					\
			write(x_, t);			\
		} else {					\
			op(a_);					\
		}

#define op_ora(x)	set_nz(x |= *operand)
#define op_and(x)	set_nz(x &= *operand)
#define op_eor(x)	set_nz(x ^= *operand)
		case Operation::ORA:	index(op_ora);		break;
		case Operation::AND:	index(op_and);		break;
		case Operation::EOR:	index(op_eor);		break;
#undef op_eor
#undef op_and
#undef op_ora
#undef index

#define op_cmp(x)	{								\
			const uint16_t temp16 = x - *operand;	\
			set_nz(uint8_t(temp16));				\
			carry_flag_ = (~temp16 >> 8)&1;			\
		}
		case Operation::CMP:
			if(index_mode_) {
				op_cmp(read(x_));
			} else {
				op_cmp(a_);
			}
		break;
		case Operation::CPX:	op_cmp(x_);			break;
		case Operation::CPY:	op_cmp(y_);			break;
#undef op_cmp

		case Operation::SBC:
		case Operation::ADC: {
			const uint8_t a = index_mode_ ? read(x_) : a_;

			if(decimal_mode_) {
				if(operation == Operation::ADC) {
					uint16_t partials = 0;
					int result = carry_flag_;

#define nibble(mask, limit, adjustment, carry)		\
	result += (a & mask) + (*operand & mask);		\
	partials += result & mask;						\
	if(result >= limit) result = ((result + (adjustment)) & (carry - 1)) + carry;

					nibble(0x000f, 0x000a, 0x0006, 0x00010);
					nibble(0x00f0, 0x00a0, 0x0060, 0x00100);

#undef nibble

					overflow_result_ = uint8_t((partials ^ a) & (partials ^ *operand));
					set_nz(uint8_t(result));
					carry_flag_ = (result >> 8) & 1;
				} else {
					unsigned int result = 0;
					unsigned int borrow = carry_flag_ ^ 1;
					const uint16_t decimal_result = uint16_t(a - *operand - borrow);

#define nibble(mask, adjustment, carry)					\
	result += (a & mask) - (*operand & mask) - borrow;	\
	if(result > mask) result -= adjustment;				\
	borrow = (result > mask) ? carry : 0;				\
	result &= (carry - 1);

					nibble(0x000f, 0x0006, 0x00010);
					nibble(0x00f0, 0x0060, 0x00100);

#undef nibble

					overflow_result_ = uint8_t((decimal_result ^ a) & (~decimal_result ^ *operand));
					set_nz(uint8_t(result));
					carry_flag_ = ((borrow >> 8)&1)^1;
				}
			} else {
				int result;
				if(operation == Operation::ADC) {
					result = int(a + *operand + carry_flag_);
					overflow_result_ = uint8_t((result ^ a) & (result ^ *operand));
				} else {
					result = int(a + ~*operand + carry_flag_);
					overflow_result_ = uint8_t((result ^ a) & (result ^ ~*operand));
				}
				set_nz(uint8_t(result));
				carry_flag_ = (result >> 8) & 1;
			}

			if(index_mode_) {
				write(x_, a);
			} else {
				a_ = a;
			}
		} break;


		/*
			Already removed from the instruction stream:

				* all branches and jumps;
				* LDM.
		*/

		default:
			printf("Unimplemented operation: %d\n", int(operation));
			assert(false);
	}
#undef set_nz
}
