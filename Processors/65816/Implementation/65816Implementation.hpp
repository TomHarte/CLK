//
//  65816Implementation.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 27/09/2020.
//  Copyright © 2020 Thomas Harte. All rights reserved.
//

template <typename BusHandler> void Processor<BusHandler>::run_for(const Cycles cycles) {
	// Temporary storage for the next bus cycle.
	uint32_t bus_address = 0;
	uint8_t *bus_value = nullptr;
	uint8_t throwaway = 0;
	BusOperation bus_operation = BusOperation::None;

#define perform_bus(address, value, operation)	\
	bus_address = address;						\
	bus_value = value;							\
	bus_operation = operation

#define read(address, value)	perform_bus(address, value, MOS6502Esque::Read)
#define write(address, value)	perform_bus(address, value, MOS6502Esque::Write)

#define m_flag() mx_flags_[0]
#define x_flag() mx_flags_[1]

#define x()	(x_.full & x_masks_[1])
#define y()	(y_.full & x_masks_[1])

	Cycles number_of_cycles = cycles + cycles_left_to_run_;
	while(number_of_cycles > Cycles(0)) {
		const MicroOp operation = *next_op_;
		++next_op_;

		switch(operation) {

			//
			// Scheduling.
			//

			case OperationMoveToNextProgram: {
				// The exception program will determine the appropriate way to respond
				// based on the pending exception if one exists; otherwise just do a
				// standard fetch-decode-execute.
				const auto offset = instructions[pending_exceptions_ ? size_t(OperationSlot::Exception) : size_t(OperationSlot::FetchDecodeExecute)].program_offsets[0];
				next_op_ = &micro_ops_[offset];
				instruction_buffer_.clear();
				data_buffer_.clear();
				last_operation_pc_ = pc_;
			} continue;

			case OperationDecode: {
				active_instruction_ = &instructions[instruction_buffer_.value];

				const auto size_flag = mx_flags_[active_instruction_->size_field];
				next_op_ = &micro_ops_[active_instruction_->program_offsets[size_flag]];
				instruction_buffer_.clear();
			} continue;

			//
			// PC fetches.
			//

			case CycleFetchIncrementPC:
				read(pc_ | program_bank_, instruction_buffer_.next_input());
				++pc_;
			break;

			case CycleFetchOpcode:
				perform_bus(pc_ | program_bank_, instruction_buffer_.next_input(), MOS6502Esque::ReadOpcode);
				++pc_;
			break;

			case CycleFetchPC:
				read(pc_ | program_bank_, instruction_buffer_.next_input());
			break;

			case CycleFetchPCThrowaway:
				read(pc_ | program_bank_, &throwaway);
			break;

			//
			// Data fetches and stores.
			//

#define increment_data_address() data_address_ = (data_address_ & 0xff0000) + ((data_address_ + 1) & 0xffff)
#define decrement_data_address() data_address_ = (data_address_ & 0xff0000) + ((data_address_ - 1) & 0xffff)


			case CycleFetchData:
				read(data_address_, data_buffer_.next_input());
			break;

			case CycleFetchIncorrectDataAddress:
				read(incorrect_data_address_, &throwaway);
			break;

			case CycleFetchIncrementData:
				read(data_address_, data_buffer_.next_input());
				increment_data_address();
			break;

			case CycleStoreData:
				write(data_address_, data_buffer_.next_output());
			break;

			case CycleStoreIncrementData:
				write(data_address_, data_buffer_.next_output());
				increment_data_address();
			break;

			case CycleStoreDecrementData:
				write(data_address_, data_buffer_.next_output());
				decrement_data_address();
			break;

			case CycleFetchBlockX:
				read(((instruction_buffer_.value & 0xff00) << 8) | x(), data_buffer_.any_byte());
			break;

			case CycleFetchBlockY:
				read(((instruction_buffer_.value & 0xff00) << 8) | y(), &throwaway);
			break;

			case CycleStoreBlockY:
				write(((instruction_buffer_.value & 0xff00) << 8) | x(), data_buffer_.any_byte());
			break;

#undef increment_data_address
#undef decrement_data_address

			//
			// Stack accesses.
			//

#define stack_access(value, operation)	\
	if(emulation_flag_) {	\
		bus_address = s_.halves.low | 0x100;	\
	} else {	\
		bus_address = s_.full;	\
	}	\
	bus_value = value;	\
	bus_operation = operation;

			case CyclePush:
				stack_access(data_buffer_.next_stack(), MOS6502Esque::Write);
				--s_.full;
			break;

			case CyclePull:
				++s_.full;
				stack_access(data_buffer_.next_input(), MOS6502Esque::Read);
			break;

			case CycleAccessStack:
				stack_access(&throwaway, MOS6502Esque::Read);
			break;

#undef stack_access

			//
			// Data movement.
			//

			case OperationCopyPCToData:
				data_buffer_.size = 2;
				data_buffer_.value = pc_;
			continue;

			case OperationCopyInstructionToData:
				data_buffer_ = instruction_buffer_;
			continue;

			case OperationCopyAToData:
				if(m_flag()) {
					data_buffer_.size = 1;
					data_buffer_.value = a_.halves.high;
				} else {
					data_buffer_.size = 2;
					data_buffer_.value = a_.full;
				}
			continue;

			case OperationCopyDataToA:
				if(m_flag()) {
					a_.halves.high = data_buffer_.value;
				} else {
					a_.full = data_buffer_.value;
				}
			continue;

			case OperationCopyPBRToData:
				data_buffer_.size = 1;
				data_buffer_.value = program_bank_ >> 16;
			continue;

			case OperationCopyDataToPC:
				pc_ = uint16_t(data_buffer_.value);
			continue;

			//
			// Address construction.
			//

			case OperationConstructAbsolute:
				data_address_ = instruction_buffer_.value + data_bank_;
			break;

			case OperationConstructAbsoluteIndexedIndirect:
				data_address_ = (instruction_buffer_.value + x()) & 0xffff;
			continue;

			case OperationConstructAbsoluteLongX:
				data_address_ = (instruction_buffer_.value + x()) & 0xffff + instruction_buffer_.value & 0xff0000;
			continue;

			case OperationConstructAbsoluteXRead:
			case OperationConstructAbsoluteX:
				data_address_ = ((instruction_buffer_.value + x()) & 0xffff) + data_bank_;
				incorrect_data_address_ = (data_address_ & 0xff) | (instruction_buffer_.value & 0xff00) + data_bank_;

				// If the incorrect address isn't actually incorrect, skip its usage.
				if(operation == OperationConstructAbsoluteXRead && data_address_ == incorrect_data_address_) {
					++next_op_;
				}
			continue;

			case OperationConstructAbsoluteYRead:
			case OperationConstructAbsoluteY:
				data_address_ = ((instruction_buffer_.value + y()) & 0xffff) + data_bank_;
				incorrect_data_address_ = (data_address_ & 0xff) + (instruction_buffer_.value & 0xff00) + data_bank_;

				// If the incorrect address isn't actually incorrect, skip its usage.
				if(operation == OperationConstructAbsoluteYRead && data_address_ == incorrect_data_address_) {
					++next_op_;
				}
			continue;

			case OperationConstructDirect:
				data_address_ = (direct_ + instruction_buffer_.value) & 0xffff;
				if(!(direct_&0xff)) {
					++next_op_;
				}
			continue;

			case OperationConstructDirectIndexedIndirect:
				data_address_ = data_bank_ + (direct_ + x() + instruction_buffer_.value) & 0xffff;
				if(!(direct_&0xff)) {
					++next_op_;
				}
			continue;

			case OperationConstructDirectIndirect:
				data_address_ = data_bank_ + (direct_ + instruction_buffer_.value) & 0xffff;
				if(!(direct_&0xff)) {
					++next_op_;
				}
			continue;

			case OperationConstructDirectIndirectIndexedLong:
				// TODO: assumed here is that the low 16-bit calculation can't carry into
				// the high byte. Test this!
				data_address_ = (y() + instruction_buffer_.value) & 0xffff + instruction_buffer_.value & 0xff0000;
			continue;

			case OperationConstructDirectIndirectLong:
				data_address_ = instruction_buffer_.value;
			continue;

			case OperationConstructDirectX:
				data_address_ = (instruction_buffer_.value + direct_ + x()) & 0xffff;
				incorrect_data_address_ = (direct_ & 0xff00) + (data_address_ & 0x00ff);
				if(!(direct_&0xff)) {
					++next_op_;
				}
			continue;

			case OperationConstructDirectY:
				data_address_ = (instruction_buffer_.value + direct_ + y()) & 0xffff;
				// TODO: given the 16-bit internal arithmetic, confirm this is the correct spurious address.
				incorrect_data_address_ = (direct_ & 0xff00) + (data_address_ & 0x00ff);
				if(!(direct_&0xff)) {
					++next_op_;
				}
			continue;

			case OperationConstructPER:
				data_buffer_.value = instruction_buffer_.value + pc_;
				data_buffer_.size = 2;
			continue;

			case OperationConstructStackRelative:
				data_address_ = (s_.full + instruction_buffer_.value) & 0xffff;
			continue;

			case OperationConstructStackRelativeIndexedIndirect:
				data_address_ = data_bank_ + (instruction_buffer_.value + y()) & 0xffff;
			continue;

			case OperationPrepareException: {
				// Put the proper exception vector into the data address, put the flags and PC
				// into the data buffer (possibly also PBR), and skip an instruction if in
				// emulation mode.

				bool is_brk = false;

				if(pending_exceptions_ & (Reset | PowerOn)) {
					// TODO: set emulation mode, etc.
					pending_exceptions_ &= ~(Reset | PowerOn);
					data_address_ = 0xfffc;
				} else if(pending_exceptions_ & NMI) {
					pending_exceptions_ &= ~NMI;
					data_address_ = 0xfffa;
				} else if(pending_exceptions_ & IRQ) {
					pending_exceptions_ &= ~IRQ;
					data_address_ = 0xfffe;
				} else {
					is_brk = active_instruction_ == instructions;
					if(is_brk) {
						data_address_ = emulation_flag_ ? 0xfffe : 0xfff6;
					} else {
						// Implicitly: COP.
						data_address_ = 0xfff4;
					}
				}

				data_buffer_.value = (pc_ << 8) | flags_.get();
				if(emulation_flag_) {
					if(is_brk) data_buffer_.value |= Flag::Break;
					data_buffer_.size = 3;
					++next_op_;
				} else {
					data_buffer_.value |= program_bank_ << 24;
					data_buffer_.size = 4;
					program_bank_ = 0;

					assert(false);	// TODO: proper flags, still.
				}

				flags_.inverse_interrupt = 0;
			} continue;

			//
			// Performance.
			//

#define LD(dest, src, masks) dest.full = (dest.full & masks[0]) | (src & masks[1])
#define m_top() (instruction_buffer_.value >> m_shift_) & 0xff
#define x_top() (x_.full >> x_shift_) & 0xff
#define y_top() (y_.full >> x_shift_) & 0xff
#define a_top() (a_.full >> m_shift_) & 0xff

			case OperationPerform:
				switch(active_instruction_->operation) {

					//
					// Loads, stores and transfers (and NOP).
					//

					case LDA:
						LD(a_, data_buffer_.value, m_masks_);
						flags_.set_nz(a_.full, m_shift_);
					break;

					case LDX:
						LD(x_, data_buffer_.value, x_masks_);
						flags_.set_nz(x_.full, x_shift_);
					break;

					case LDY:
						LD(y_, data_buffer_.value, x_masks_);
						flags_.set_nz(y_.full, x_shift_);
					break;

					case PLB:
						data_bank_ = (data_buffer_.value & 0xff) << 16;
						flags_.set_nz(instruction_buffer_.value);
					break;

					case PLD:
						direct_ = data_buffer_.value;
						flags_.set_nz(instruction_buffer_.value);
					break;

					case PLP:
						flags_.set(data_buffer_.value);

						if(!emulation_flag_) {
							assert(false);	// TODO: M and X.
						}
					break;

					case STA:
						data_buffer_.value = a_.full & m_masks_[1];
						data_buffer_.size = 2 - m_flag();
					break;

					case STZ:
						data_buffer_.value = 0;
						data_buffer_.size = 2 - m_flag();
					break;

					case STX:
						data_buffer_.value = x_.full & x_masks_[1];
						data_buffer_.size = 2 - x_flag();
					break;

					case STY:
						data_buffer_.value = y_.full & x_masks_[1];
						data_buffer_.size = 2 - m_flag();
					break;

					case PHB:
						data_buffer_.value = data_bank_ >> 16;
						data_buffer_.size = 1;
					break;

					case PHK:
						data_buffer_.value = program_bank_ >> 16;
						data_buffer_.size = 1;
					break;

					case PHD:
						data_buffer_.value = direct_;
						data_buffer_.size = 2;
					break;

					case PHP:
						data_buffer_.value = flags_.get();
						data_buffer_.size = 1;

						if(!emulation_flag_) {
							assert(false);	// TODO: M and X.
						} else {
							// On the 6502, the break flag is set during a PHP.
							data_buffer_.value |= Flag::Break;
						}
					break;

					case NOP:	break;

					// The below attempt to obey the 8/16-bit mixed transfer rules
					// as documented in https://softpixel.com/~cwright/sianse/docs/65816NFO.HTM
					// (and makes reasonable guesses as to the N flag)

					case TXS:
						s_ = x_.full & x_masks_[1];
					break;

					case TSX:
						LD(x_, s_.full, x_masks_);
						flags_.set_nz(x_.full, x_shift_);
					break;

					case TXY:
						LD(y_, x_.full, x_masks_);
						flags_.set_nz(y_.full, x_shift_);
					break;

					case TYX:
						LD(x_, y_.full, x_masks_);
						flags_.set_nz(x_.full, x_shift_);
					break;

					case TAX:
						LD(x_, a_.full, x_masks_);
						flags_.set_nz(x_.full, x_shift_);
					break;

					case TAY:
						LD(y_, a_.full, x_masks_);
						flags_.set_nz(y_.full, x_shift_);
					break;

					case TXA:
						LD(a_, x_.full, m_masks_);
						flags_.set_nz(a_.full, m_shift_);
					break;

					case TYA:
						LD(a_, y_.full, m_masks_);
						flags_.set_nz(a_.full, m_shift_);
					break;


					//
					// Jumps and returns.
					//

					case JML:
						program_bank_ = instruction_buffer_.value & 0xff0000;
					[[fallthrough]];

					case JMP:
						pc_ = uint16_t(instruction_buffer_.value);
					break;

					case JMPind:
						pc_ = data_buffer_.value;
					break;

					case RTS:
						pc_ = data_buffer_.value + 1;
					break;

					case JSL:
						program_bank_ = instruction_buffer_.value & 0xff0000;
					[[fallthrough]];

					case JSR:
						data_buffer_.value = pc_;
						data_buffer_.size = 2;

						pc_ = instruction_buffer_.value;
					break;

					case RTI:
						pc_ = uint16_t(data_buffer_.value >> 8);
						flags_.set(uint8_t(data_buffer_.value));

						if(!emulation_flag_) {
							program_bank_ = (data_buffer_.value & 0xff000000) >> 8;
							assert(false); // Extra flags to unpack!
						}
					break;

					//
					// Block moves.
					//

					case MVP:
						data_bank_ = (instruction_buffer_.value & 0xff) << 16;
						--x_.full;
						--y_.full;
						--a_.full;
						if(a_.full) pc_ -= 3;
					break;

					case MVN:
						data_bank_ = (instruction_buffer_.value & 0xff) << 16;
						++x_.full;
						++y_.full;
						--a_.full;
						if(a_.full) pc_ -= 3;
					break;

					//
					// Flag manipulation.
					//

					case CLC: flags_.carry = 0;								break;
					case CLI: flags_.inverse_interrupt = Flag::Interrupt;	break;
					case CLV: flags_.overflow = 0;							break;
					case CLD: flags_.decimal = 0;							break;

					case SEC: flags_.carry = Flag::Carry;					break;
					case SEI: flags_.inverse_interrupt = 0;					break;
					case SED: flags_.decimal = Flag::Decimal;				break;

					//
					// Increments and decrements.
					//

					case INC:
						++instruction_buffer_.value;
						flags_.set_nz(instruction_buffer_.value, m_shift_);
					break;;

					case DEC:
						--instruction_buffer_.value;
						flags_.set_nz(instruction_buffer_.value, m_shift_);
					break;

					case INX: {
						const uint16_t x_inc = x_.full + 1;
						LD(x_, x_inc, x_masks_);
						flags_.set_nz(x_.full, x_shift_);
					} break;

					case DEX: {
						const uint16_t x_dec = x_.full - 1;
						LD(x_, x_dec, x_masks_);
						flags_.set_nz(x_.full, x_shift_);
					} break;

					case INY: {
						const uint16_t y_inc = y_.full + 1;
						LD(y_, y_inc, x_masks_);
						flags_.set_nz(y_.full, x_shift_);
					} break;

					case DEY: {
						const uint16_t y_dec = y_.full - 1;
						LD(y_, y_dec, x_masks_);
						flags_.set_nz(y_.full, x_shift_);
					} break;

					//
					// Bitwise operations.
					//

					case AND:
						a_.full &= instruction_buffer_.value | m_masks_[0];
						flags_.set_nz(a_.full, m_shift_);
					break;

					case EOR:
						a_.full ^= instruction_buffer_.value;
						flags_.set_nz(a_.full, m_shift_);
					break;

					case ORA:
						a_.full |= instruction_buffer_.value;
						flags_.set_nz(a_.full, m_shift_);
					break;

					case BIT:
						flags_.set_nz(data_buffer_.value & a_.full, m_shift_);
						flags_.overflow = data_buffer_.value & Flag::Overflow;
					break;

					case BITimm:
						flags_.set_z(data_buffer_.value & a_.full, m_shift_);
					break;

					//
					// Branches.
					//

#define BRA(condition)	\
	if(!(condition)) {	\
		next_op_ += 3;	\
	} else {			\
		data_buffer_.size = 2;	\
		data_buffer_.value = pc_ + int8_t(instruction_buffer_.value);	\
																		\
		if((pc_ & 0xff00) == (instruction_buffer_.value & 0xff00)) {	\
			++next_op_;													\
		}																\
	}

					case BPL: BRA(!(flags_.negative_result&0x80));	break;
					case BMI: BRA(flags_.negative_result&0x80);		break;
					case BVC: BRA(!flags_.overflow);				break;
					case BVS: BRA(flags_.overflow);					break;
					case BCC: BRA(!flags_.carry);					break;
					case BCS: BRA(flags_.carry);					break;
					case BNE: BRA(flags_.zero_result);				break;
					case BEQ: BRA(!flags_.zero_result);				break;
					case BRA: BRA(true);							break;

#undef BRA

					case BRL:
						pc_ += int16_t(instruction_buffer_.value);
					break;

					//
					// Shifts and rolls.
					//

					case ASL:
						flags_.carry = data_buffer_.value >> (7 + m_shift_);
						data_buffer_.value <<= 1;
						flags_.set_nz(instruction_buffer_.value, m_shift_);
					break;

					case LSR:
						flags_.carry = data_buffer_.value & 1;
						data_buffer_.value >>= 1;
						flags_.set_nz(instruction_buffer_.value, m_shift_);
					break;

					case ROL:
						data_buffer_.value = (data_buffer_.value << 1) | flags_.carry;
						flags_.carry = data_buffer_.value >> (7 + m_shift_);
						flags_.set_nz(instruction_buffer_.value, m_shift_);
					break;

					case ROR: {
						const uint8_t next_carry = data_buffer_.value & 1;
						data_buffer_.value = (data_buffer_.value >> 1) | (flags_.carry << (7 + m_shift_));
						flags_.carry = next_carry;
						flags_.set_nz(instruction_buffer_.value, m_shift_);
					} break;

					//
					// Arithmetic.
					//

#define cp(v, shift, masks)	{\
	const uint32_t temp32 = (v.full & masks[1]) - (data_buffer_.value & masks[1]);	\
	flags_.set_nz(uint16_t(temp32), shift);	\
	flags_.carry = ((~temp32) >> (8 + shift))&1;	\
}

					case CMP:	cp(a_, m_shift_, m_masks_);	break;
					case CPX:	cp(x_, x_shift_, x_masks_);	break;
					case CPY:	cp(y_, x_shift_, x_masks_);	break;

#undef cp

					case SBC:
						data_buffer_.value = ~data_buffer_.value & m_masks_[1];
					[[fallthrough]];

					case ADC: {
						int result;
						const uint16_t a = a_.full & m_masks_[1];

						if(flags_.decimal) {
							result = flags_.carry;
							const int nibble_adjustment = (active_instruction_->operation == SBC) ? 0xa : 0x6;

							// TODO: this still isn't quite correct for SBC as the limit test is wrong, I think.

#define nibble(mask, limit, addition, carry)			\
	result += (a & mask) + (data_buffer_.value & mask);	\
	if(result >= limit) result = ((result + (addition)) & (carry - 1)) + carry;

							nibble(0x000f, 0x000a, nibble_adjustment << 0, 0x00010);
							nibble(0x00f0, 0x00a0, nibble_adjustment << 8, 0x00100);
							nibble(0x0f00, 0x0a00, nibble_adjustment << 16, 0x01000);
							nibble(0xf000, 0xa000, nibble_adjustment << 24, 0x10000);

#undef nibble

						} else {
							result = a + data_buffer_.value + flags_.carry;
						}

						flags_.overflow = (( (result ^ a_.full) & (result ^ data_buffer_.value) ) >> (1 + m_shift_))&0x40;
						flags_.set_nz(result, m_shift_);
						flags_.carry = (result >> (8 + m_shift_))&1;
						LD(a_, result, m_masks_);
					} break;

					// TODO:
					//	TRB, TSB,
					//	REP, SEP,
					//	XCE, XBA,
					//	STP, WAI,
					//	RTL,
					//	TCD, TCS, TDC, TSC

					default:
						assert(false);
				}
			continue;

			default:
				assert(false);
		}

#undef LD
#undef m_top
#undef x_top
#undef y_top
#undef a_top

		number_of_cycles -= bus_handler_.perform_bus_operation(bus_operation, bus_address, bus_value);
	}

#undef read
#undef write
#undef bus_operation
#undef x
#undef y
#undef m_flag
#undef x_flag

	cycles_left_to_run_ = number_of_cycles;
}

void ProcessorBase::set_power_on(bool active) {
	if(active) {
		pending_exceptions_ |= PowerOn;
	} else {
		pending_exceptions_ &= ~PowerOn;
	}
}

void ProcessorBase::set_irq_line(bool active) {
	if(active) {
		pending_exceptions_ |= IRQ;
	} else {
		pending_exceptions_ &= ~IRQ;
	}
}

void ProcessorBase::set_reset_line(bool active) {
	if(active) {
		pending_exceptions_ |= Reset;
	} else {
		pending_exceptions_ &= ~Reset;
	}
}

void ProcessorBase::set_nmi_line(bool active) {
	// This is edge triggered.
	if(active) {
		pending_exceptions_ |= NMI;
	}
}

// The 65816 can't jam.
bool ProcessorBase::is_jammed() const { return false; }
