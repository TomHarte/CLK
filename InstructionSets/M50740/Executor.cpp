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

#include "../../Machines/Utility/MemoryFuzzer.hpp"

#define LOG_PREFIX "[M50740] "
#include "../../Outputs/Log.hpp"

using namespace InstructionSet::M50740;

namespace {
	constexpr int port_remap[] = {0, 1, 2, 0, 3};
}

Executor::Executor(PortHandler &port_handler) : port_handler_(port_handler) {
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

	// Fuzz RAM; then set anything that may be replaced by ROM to FF.
	Memory::Fuzz(memory_);
	memset(&memory_[0x100], 0xff, memory_.size() - 0x100);
}

void Executor::set_rom(const std::vector<uint8_t> &rom) {
	// Copy into place, and reset.
	const auto length = std::min(size_t(0x1000), rom.size());
	memcpy(&memory_[0x2000 - length], rom.data(), length);
	reset();
}

void Executor::run_for(Cycles cycles) {
	// The incoming clock is divided by four; the local cycles_ count
	// ensures that fractional parts are kept track of.
	cycles_ += cycles;
	CachingExecutor::run_for(cycles_.divide(Cycles(4)).as<int>());
}

void Executor::reset() {
	// Just jump to the reset vector.
	set_program_counter(uint16_t(memory_[0x1ffe] | (memory_[0x1fff] << 8)));
}

void Executor::set_interrupt_line(bool line) {
	// Super hack: interrupt now, if permitted. Otherwise do nothing.
	// So this will fail to catch enabling of interrupts while the line
	// is active, amongst other things.
	if(interrupt_line_ != line) {
		interrupt_line_ = line;

		// TODO: verify interrupt connection. Externally, but stubbed off here.
//		if(!interrupt_disable_ && line) {
//			perform_interrupt<false>(0x1ff4);
//		}
	}
}

uint8_t Executor::read(uint16_t address) {
	address &= 0x1fff;

	// Deal with RAM and ROM accesses quickly.
	if(address < 0x60 || address >= 0x100) return memory_[address];

	port_handler_.run_ports_for(cycles_since_port_handler_.flush<Cycles>());
	switch(address) {
		default:
			LOG("Unrecognised read from " << PADHEX(4) << address);
		return 0xff;

		// "Port R"; sixteen four-bit ports
		case 0xd0: case 0xd1: case 0xd2: case 0xd3: case 0xd4: case 0xd5: case 0xd6: case 0xd7:
		case 0xd8: case 0xd9: case 0xda: case 0xdb: case 0xdc: case 0xdd: case 0xde: case 0xdf:
			LOG("Unimplemented Port R read from " << PADHEX(4) << address);
		return 0x00;

		// Ports P0–P3.
		case 0xe0: case 0xe2:
		case 0xe4: case 0xe8: {
			const int port = port_remap[(address - 0xe0) >> 1];
			const uint8_t input = port_handler_.get_port_input(port);

			// In the direction registers, a 0 indicates input, a 1 indicates output.
			return (input &~ port_directions_[port]) | (port_outputs_[port] & port_directions_[port]);
		}

		case 0xe1: case 0xe3:
		case 0xe5: case 0xe9:
		return port_directions_[port_remap[(address - 0xe0) >> 1]];

		// Timers.
		case 0xf9:	return prescalers_[0].value;
		case 0xfa:	return timers_[0].value;
		case 0xfb:	return timers_[1].value;
		case 0xfc:	return prescalers_[1].value;
		case 0xfd:	return timers_[2].value;

		case 0xfe:	return interrupt_control_;
		case 0xff:	return timer_control_;
	}
}

void Executor::set_port_output(int port) {
	// Force 'output' to a 1 anywhere that a bit is set as input.
	port_handler_.set_port_output(port, port_outputs_[port] | ~port_directions_[port]);
}

void Executor::write(uint16_t address, uint8_t value) {
	address &= 0x1fff;

	// RAM writes are easy.
	if(address < 0x60) {
		memory_[address] = value;
		return;
	}

	// ROM 'writes' are almost as easy (albeit unexpected).
	if(address >= 0x100) {
		LOG("Attempted ROM write of " << PADHEX(2) << value << " to " << PADHEX(4) << address);
		return;
	}

	// Push time to the port handler.
	port_handler_.run_ports_for(cycles_since_port_handler_.flush<Cycles>());

	switch(address) {
		default:
			LOG("Unrecognised write of " << PADHEX(2) << value << " to " << PADHEX(4) << address);
		break;

		// "Port R"; sixteen four-bit ports
		case 0xd0: case 0xd1: case 0xd2: case 0xd3: case 0xd4: case 0xd5: case 0xd6: case 0xd7:
		case 0xd8: case 0xd9: case 0xda: case 0xdb: case 0xdc: case 0xdd: case 0xde: case 0xdf:
			LOG("Unimplemented Port R write of " << PADHEX(2) << value << " from " << PADHEX(4) << address);
		break;

		// Ports P0–P3.
		case 0xe0: case 0xe2:
		case 0xe4: case 0xe8: {
			const int port = port_remap[(address - 0xe0) >> 1];
			port_outputs_[port] = value;
			set_port_output(port);
		}
		break;

		case 0xe1: case 0xe3:
		case 0xe5: case 0xe9: {
			const int port = port_remap[(address - 0xe0) >> 1];
			port_directions_[port] = value;
			set_port_output(port);
		} break;

		// Timers.
		case 0xf9:	prescalers_[0].reload_value = value;	break;
		case 0xfa:	timers_[0].reload_value = value;		break;
		case 0xfb:	timers_[1].reload_value = value;		break;
		case 0xfc:	prescalers_[1].reload_value = value;	break;
		case 0xfd:	timers_[2].reload_value = value;		break;

		case 0xfe:	interrupt_control_ = value;				break;
		case 0xff:	timer_control_ = value;					break;
	}
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

template<bool is_brk> inline void Executor::perform_interrupt(uint16_t vector) {
	// BRK has an unused operand.
	++program_counter_;
	push(uint8_t(program_counter_ >> 8));
	push(uint8_t(program_counter_ & 0xff));
	push(flags() | (is_brk ? 0x10 : 0x00));
	set_program_counter(uint16_t(memory_[vector] | (memory_[vector+1] << 8)));
}

template <Operation operation, AddressingMode addressing_mode> void Executor::perform() {
	// Post cycle cost; this emulation _does not provide accurate timing_.
#define TLength(mode, base)	case AddressingMode::mode: subtract_duration(base + t_lengths[index_mode_]); break;
#define Length(mode, base)	case AddressingMode::mode: subtract_duration(base); break;

	switch(operation) {
		case Operation::ADC:	case Operation::AND:	case Operation::CMP:	case Operation::EOR:
		case Operation::LDA:	case Operation::ORA:	case Operation::SBC:
		{
			constexpr int t_lengths[] = {
				0,
				operation == Operation::LDA ? 2 : (operation == Operation::CMP ? 1 : 3)
			};
			switch(addressing_mode) {
				TLength(XIndirect, 6);
				TLength(ZeroPage, 3);
				TLength(Immediate, 2);
				TLength(Absolute, 4);
				TLength(IndirectY, 6);
				TLength(ZeroPageX, 4);
				TLength(AbsoluteY, 5);
				TLength(AbsoluteX, 5);
				default: assert(false);
			}
		} break;

		case Operation::ASL:	case Operation::DEC:	case Operation::INC:	case Operation::LSR:
		case Operation::ROL:	case Operation::ROR:
			switch(addressing_mode) {
				Length(ZeroPage, 5);
				Length(Accumulator, 2);
				Length(Absolute, 6);
				Length(ZeroPageX, 6);
				Length(AbsoluteX, 7);
				default: assert(false);
			}
		break;

		case Operation::BBC0:	case Operation::BBC1:	case Operation::BBC2:	case Operation::BBC3:
		case Operation::BBC4:	case Operation::BBC5:	case Operation::BBC6:	case Operation::BBC7:
		case Operation::BBS0:	case Operation::BBS1:	case Operation::BBS2:	case Operation::BBS3:
		case Operation::BBS4:	case Operation::BBS5:	case Operation::BBS6:	case Operation::BBS7:
			switch(addressing_mode) {
				Length(AccumulatorRelative, 4);
				Length(ZeroPageRelative, 5);
				default: assert(false);
			}
		break;
		case Operation::BPL:	case Operation::BMI:	case Operation::BEQ:	case Operation::BNE:
		case Operation::BCS:	case Operation::BCC:	case Operation::BVS:	case Operation::BVC:
		case Operation::INX:	case Operation::INY:
			subtract_duration(2);
		break;

		case Operation::CPX:	case Operation::CPY:	case Operation::BIT:	case Operation::LDX:
		case Operation::LDY:
			switch(addressing_mode) {
				Length(Immediate, 2);
				Length(ZeroPage, 3);
				Length(Absolute, 4);
				Length(ZeroPageX, 4);
				Length(ZeroPageY, 4);
				Length(AbsoluteX, 5);
				Length(AbsoluteY, 5);
				default: assert(false);
			}
		break;

		case Operation::BRA:	subtract_duration(4);	break;
		case Operation::BRK:	subtract_duration(7);	break;

		case Operation::CLB0:	case Operation::CLB1:	case Operation::CLB2:	case Operation::CLB3:
		case Operation::CLB4:	case Operation::CLB5:	case Operation::CLB6:	case Operation::CLB7:
		case Operation::SEB0:	case Operation::SEB1:	case Operation::SEB2:	case Operation::SEB3:
		case Operation::SEB4:	case Operation::SEB5:	case Operation::SEB6:	case Operation::SEB7:
			switch(addressing_mode) {
				Length(Accumulator, 2);
				Length(ZeroPage, 5);
				default: assert(false);
			}
		break;

		case Operation::CLC:	case Operation::CLD:	case Operation::CLT:	case Operation::CLV:
		case Operation::CLI:
		case Operation::DEX:	case Operation::DEY:	case Operation::FST:	case Operation::NOP:
		case Operation::SEC:	case Operation::SED:	case Operation::SEI:	case Operation::SET:
		case Operation::SLW:	case Operation::STP:	case Operation::TAX:	case Operation::TAY:
		case Operation::TSX:	case Operation::TXA:	case Operation::TXS:	case Operation::TYA:
			subtract_duration(2);
		break;

		case Operation::COM:	subtract_duration(5);	break;

		case Operation::JMP:
			switch(addressing_mode) {
				Length(Absolute, 3);
				Length(AbsoluteIndirect, 5);
				Length(ZeroPageIndirect, 4);
				default: assert(false);
			}
		break;

		case Operation::JSR:
			switch(addressing_mode) {
				Length(ZeroPageIndirect, 7);
				Length(Absolute, 6);
				Length(SpecialPage, 5);
				default: assert(false);
			}
		break;

		case Operation::LDM:	subtract_duration(4);	break;

		case Operation::PHA:	case Operation::PHP:	case Operation::TST:
			subtract_duration(3);
		break;

		case Operation::PLA:	case Operation::PLP:
			subtract_duration(4);
		break;

		case Operation::RRF:	subtract_duration(8);	break;
		case Operation::RTI:	subtract_duration(6);	break;
		case Operation::RTS:	subtract_duration(6);	break;

		case Operation::STA:	case Operation::STX:	case Operation::STY:
			switch(addressing_mode) {
				Length(XIndirect, 7);
				Length(ZeroPage, 4);
				Length(Absolute, 5);
				Length(IndirectY, 7);
				Length(ZeroPageX, 5);
				Length(ZeroPageY, 5);
				Length(AbsoluteY, 6);
				Length(AbsoluteX, 6);
				default: assert(false);
			}
		break;

		default: assert(false);
	}

	// Deal with all modes that don't access memory up here;
	// those that access memory will go through a slightly longer
	// sequence below that wraps the address and checks whether
	// a write is valid [if required].

	unsigned int address;
#define next8()		memory_[(program_counter_ + 1) & 0x1fff]
#define next16()	uint16_t(memory_[(program_counter_ + 1) & 0x1fff] | (memory_[(program_counter_ + 2) & 0x1fff] << 8))

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
				address = unsigned(program_counter_ + 1 + size(addressing_mode) + int8_t(next8()));
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
					address = unsigned(program_counter_ + 1 + size(addressing_mode) + int8_t(next8()));
				} else {
					value = read(next8());
					address = unsigned(program_counter_ + 1 + size(addressing_mode) + int8_t(memory_[(program_counter_+2)&0x1fff]));
				}
				program_counter_ += 1 + size(addressing_mode);
				switch(operation) {
					case Operation::BBS0:	case Operation::BBS1:	case Operation::BBS2:	case Operation::BBS3:
					case Operation::BBS4:	case Operation::BBS5:	case Operation::BBS6:	case Operation::BBS7: {
						if constexpr (operation >= Operation::BBS0 && operation <= Operation::BBS7) {
							constexpr uint8_t mask = 1 << (int(operation) - int(Operation::BBS0));
							if(value & mask) {
								set_program_counter(uint16_t(address));
								subtract_duration(2);
							}
						}
					} return;
					case Operation::BBC0:	case Operation::BBC1:	case Operation::BBC2:	case Operation::BBC3:
					case Operation::BBC4:	case Operation::BBC5:	case Operation::BBC6:	case Operation::BBC7: {
						if constexpr (operation >= Operation::BBC0 && operation <= Operation::BBC7) {
							constexpr uint8_t mask = 1 << (int(operation) - int(Operation::BBC0));
							if(!(value & mask)) {
								set_program_counter(uint16_t(address));
								subtract_duration(2);
							}
						}
					} return;
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
				address = unsigned(memory_[address] | (memory_[(address + 1) & 0xff] << 8));
			break;

			case AddressingMode::XIndirect:
				address = (next8() + x_) & 0xff;
				address = unsigned(memory_[address] | (memory_[(address + 1)&0xff] << 8));
			break;

			case AddressingMode::IndirectY:
				address = unsigned((memory_[next8()] | (memory_[(next8()+1)&0xff] << 8)) + y_);
			break;

			case AddressingMode::AbsoluteIndirect:
				address = next16();
				address = unsigned(memory_[address & 0x1fff] | (memory_[(address + 1) & 0x1fff] << 8));
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
			// Push one less than the actual return address.
			const auto return_address = program_counter_ - 1;
			push(uint8_t(return_address >> 8));
			push(uint8_t(return_address & 0xff));
			set_program_counter(uint16_t(address));
		} return;

#define Bcc(c)	if(c) { set_program_counter(uint16_t(address)); subtract_duration(2); } return
		case Operation::BPL:	Bcc(!(negative_result_&0x80));
		case Operation::BMI:	Bcc(negative_result_&0x80);
		case Operation::BEQ:	Bcc(!zero_result_);
		case Operation::BNE:	Bcc(zero_result_);
		case Operation::BCS:	Bcc(carry_flag_);
		case Operation::BCC:	Bcc(!carry_flag_);
		case Operation::BVS:	Bcc(overflow_result_ & 0x80);
		case Operation::BVC:	Bcc(!(overflow_result_ & 0x80));
#undef Bcc

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
			if constexpr(operation >= Operation::SEB0 && operation <= Operation::SEB7) {
				*operand |= 1 << (int(operation) - int(Operation::SEB0));
			}
		break;
		case Operation::CLB0:	case Operation::CLB1:	case Operation::CLB2:	case Operation::CLB3:
		case Operation::CLB4:	case Operation::CLB5:	case Operation::CLB6:	case Operation::CLB7:
			if constexpr(operation >= Operation::CLB0 && operation <= Operation::CLB7) {
				*operand &= ~(1 << (int(operation) - int(Operation::CLB0)));
			}
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
			perform_interrupt<true>(0x1ff4);
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
			LOG("Unimplemented operation: " << operation);
			assert(false);
	}
#undef set_nz
}

inline void Executor::subtract_duration(int duration) {
	// Update count for potential port accesses.
	cycles_since_port_handler_ += Cycles(duration);

	// Update timer 1 and 2 prescaler.
	constexpr int t12_divider = 4;		// TODO: should be 4, I think.
	constexpr int t12_multiplier = 1;

	timer_divider_ += duration * t12_multiplier;
	const int t12_ticks = update_timer(prescalers_[0], timer_divider_ / t12_divider);
	timer_divider_ &= (t12_divider-1);

	// Update timers 1 and 2. TODO: interrupts (elsewhere).
	if(update_timer(timers_[0], t12_ticks)) interrupt_control_ |= 0x20;
	if(update_timer(timers_[1], t12_ticks)) interrupt_control_ |= 0x08;

	// TODO: timer X.
//	update_timer(timers_[2], update_timer(prescalers_[1], duration));

	// Pass along.
	CachingExecutor::subtract_duration(duration);
}

inline int Executor::update_timer(Timer &timer, int count) {
	const int next_value = timer.value - count;
	if(next_value < 0) {
		// Determine how many reloads were required to get above zero.
		const int reload_value = timer.reload_value ? timer.reload_value : 256;
		const int underflow_count = 1 - next_value / reload_value;
		timer.value = uint8_t((next_value % reload_value) + timer.reload_value);
		return underflow_count;
	}
	timer.value = uint8_t(next_value);
	return 0;
}
