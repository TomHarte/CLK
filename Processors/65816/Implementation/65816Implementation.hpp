//
//  65816Implementation.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 27/09/2020.
//  Copyright © 2020 Thomas Harte. All rights reserved.
//

template <typename BusHandler, bool uses_ready_line> void Processor<BusHandler, uses_ready_line>::run_for(const Cycles cycles) {

#define perform_bus(address, value, operation)	\
	bus_address_ = address;						\
	bus_value_ = value;							\
	bus_operation_ = operation

#define read(address, value)	perform_bus(address, value, MOS6502Esque::Read)
#define write(address, value)	perform_bus(address, value, MOS6502Esque::Write)

#define m_flag() registers_.mx_flags[0]
#define x_flag() registers_.mx_flags[1]

#define stack_address()	((registers_.s.full & registers_.e_masks[1]) | (0x0100 & registers_.e_masks[0]))

	Cycles number_of_cycles = cycles + cycles_left_to_run_;
	while(number_of_cycles > Cycles(0)) {
		// Wait for ready to be inactive before proceeding.
		while(uses_ready_line && ready_line_ && number_of_cycles > Cycles(0)) {
			number_of_cycles -= bus_handler_.perform_bus_operation(BusOperation::Ready, static_cast<typename BusHandler::AddressType>(bus_address_), &bus_throwaway_);
		}

		// Process for as much time is left and/or until ready is signalled.
		while((!uses_ready_line || !ready_line_) && number_of_cycles > Cycles(0)) {
			++count_;
			if(count_ == 148933250) {
				printf("");
			}

			const MicroOp operation = *next_op_;
			++next_op_;

#ifndef NDEBUG
			// As a sanity check.
			bus_value_ = nullptr;
#endif

			switch(operation) {
				//
				// Scheduling.
				//

				case OperationMoveToNextProgram: {
					// The exception program will determine the appropriate way to respond
					// based on the pending exception if one exists; otherwise just do a
					// standard fetch-decode-execute.
					if(selected_exceptions_) {
						exception_is_interrupt_ = true;

						// Do enough quick early decoding to spot a reset.
						if(selected_exceptions_ & (Reset | PowerOn)) {
							active_instruction_ = &instructions[size_t(OperationSlot::Reset)];
						} else {
							active_instruction_ = &instructions[size_t(OperationSlot::Exception)];
						}
					} else {
						exception_is_interrupt_ = false;
						active_instruction_ = &instructions[size_t(OperationSlot::FetchDecodeExecute)];
					}

					next_op_ = &micro_ops_[active_instruction_->program_offsets[0]];
					instruction_buffer_.clear();
					data_buffer_.clear();
					last_operation_pc_ = registers_.pc;
					last_operation_program_bank_ = uint8_t(registers_.program_bank >> 16);
					memory_lock_ = false;
				} continue;

				case OperationDecode: {
					active_instruction_ = &instructions[instruction_buffer_.value];

					const auto size_flag = registers_.mx_flags[active_instruction_->size_field];
					next_op_ = &micro_ops_[active_instruction_->program_offsets[size_flag]];
					instruction_buffer_.clear();
				} continue;

				//
				// PC fetches.
				//

				case CycleFetchOpcode:
					perform_bus(registers_.pc | registers_.program_bank, instruction_buffer_.next_input(), MOS6502Esque::ReadOpcode);
					++registers_.pc;
				break;

				case CycleFetchIncrementPC:
					perform_bus(registers_.pc | registers_.program_bank, instruction_buffer_.next_input(), MOS6502Esque::ReadProgram);
					++registers_.pc;
				break;

				case CycleFetchPC:
					perform_bus(registers_.pc | registers_.program_bank, instruction_buffer_.next_input(), MOS6502Esque::ReadProgram);
				break;

				case CycleFetchPCThrowaway:
					perform_bus(registers_.pc | registers_.program_bank, &bus_throwaway_, MOS6502Esque::InternalOperationRead);
				break;

				//
				// Data fetches and stores.
				//

#define increment_data_address() data_address_ = (data_address_ & ~data_address_increment_mask_) + ((data_address_ + 1) & data_address_increment_mask_)
#define decrement_data_address() data_address_ = (data_address_ & ~data_address_increment_mask_) + ((data_address_ - 1) & data_address_increment_mask_)


				case CycleFetchData:
					read(data_address_, data_buffer_.next_input());
				break;

				case CycleFetchDataThrowaway:
					perform_bus(data_address_, &bus_throwaway_, MOS6502Esque::InternalOperationRead);
				break;

				case CycleFetchIncorrectDataAddress:
					perform_bus(incorrect_data_address_, &bus_throwaway_, MOS6502Esque::InternalOperationRead);
				break;

				case CycleFetchIncrementData:
					read(data_address_, data_buffer_.next_input());
					increment_data_address();
				break;

				case CycleFetchVector:
					perform_bus(data_address_, data_buffer_.next_input(), MOS6502Esque::ReadVector);
				break;

				case CycleFetchIncrementVector:
					perform_bus(data_address_, data_buffer_.next_input(), MOS6502Esque::ReadVector);
					increment_data_address();
				break;

				case CycleStoreData:
					write(data_address_, data_buffer_.next_output());
				break;

				case CycleStoreDataThrowaway:
					perform_bus(data_address_, data_buffer_.preview_output(), MOS6502Esque::InternalOperationWrite);
				break;

				case CycleStoreIncrementData:
					write(data_address_, data_buffer_.next_output());
					increment_data_address();
				break;

				case CycleStoreDecrementData:
					write(data_address_, data_buffer_.next_output_descending());
					decrement_data_address();
				break;

				case CycleFetchBlockX:
					read(((instruction_buffer_.value & 0xff00) << 8) | registers_.x.full, data_buffer_.any_byte());
				break;

				case CycleFetchBlockY:
					perform_bus(((instruction_buffer_.value & 0x00ff) << 16) | registers_.y.full, &bus_throwaway_, MOS6502Esque::InternalOperationRead);
				break;

				case CycleStoreBlockY:
					write(((instruction_buffer_.value & 0x00ff) << 16) | registers_.y.full, data_buffer_.any_byte());
				break;

#undef increment_data_address
#undef decrement_data_address

				//
				// Stack accesses.
				//

#define stack_access(value, operation)	\
	bus_address_ = stack_address();		\
	bus_value_ = value;					\
	bus_operation_ = operation;

				case CyclePush:
					stack_access(data_buffer_.next_output_descending(), MOS6502Esque::Write);
					--registers_.s.full;
				break;

				case CyclePullIfNotEmulation:
					if(registers_.emulation_flag) {
						continue;
					}
				[[fallthrough]];

				case CyclePull:
					++registers_.s.full;
					stack_access(data_buffer_.next_input(), MOS6502Esque::Read);
				break;

				case CycleAccessStack:
					stack_access(&bus_throwaway_, MOS6502Esque::InternalOperationRead);
				break;

#undef stack_access

				//
				// Memory lock control.
				//

				case OperationSetMemoryLock:
					memory_lock_ = true;
				continue;

				//
				// STP and WAI.
				//

				case CycleRepeatingNone:
					if(selected_exceptions_ & required_exceptions_) {
						continue;
					} else {
						--next_op_;
						perform_bus(0xffffff, &bus_throwaway_, (required_exceptions_ & IRQ) ? MOS6502Esque::Ready : MOS6502Esque::None);
					}
				break;

				//
				// Data movement.
				//

				case OperationCopyPCToData:
					data_buffer_.size = 2;
					data_buffer_.value = registers_.pc;
				continue;

				case OperationCopyInstructionToData:
					data_buffer_ = instruction_buffer_;
				continue;

				case OperationCopyDataToInstruction:
					instruction_buffer_ = data_buffer_;
					data_buffer_.clear();
				continue;

				case OperationCopyAToData:
					data_buffer_.value = registers_.a.full & registers_.m_masks[1];
					data_buffer_.size = 2 - m_flag();
				continue;

				case OperationCopyDataToA:
					registers_.a.full = (registers_.a.full & registers_.m_masks[0]) + (data_buffer_.value & registers_.m_masks[1]);
				continue;

				case OperationCopyPBRToData:
					data_buffer_.size = 1;
					data_buffer_.value = registers_.program_bank >> 16;
				continue;

				case OperationCopyDataToPC:
					registers_.pc = uint16_t(data_buffer_.value);
				continue;

				case OperationClearDataBuffer:
					data_buffer_.clear();
				continue;

				//
				// Address construction.
				//

				case OperationConstructAbsolute:
					data_address_ = instruction_buffer_.value + registers_.data_bank;
					data_address_increment_mask_ = 0xff'ff'ff;
				continue;

				case OperationConstructAbsolute16:
					data_address_ = instruction_buffer_.value;
					data_address_increment_mask_ = 0x00'ff'ff;
				continue;

				case OperationConstructAbsoluteLong:
					data_address_ = instruction_buffer_.value;
					data_address_increment_mask_ = 0xff'ff'ff;
				continue;

				// Used for JMP and JSR (absolute, x).
				case OperationConstructAbsoluteIndexedIndirect:
					data_address_ = registers_.program_bank + ((instruction_buffer_.value + registers_.x.full) & 0xffff);
					data_address_increment_mask_ = 0x00'ff'ff;
				continue;

				case OperationConstructAbsoluteLongX:
					data_address_ = instruction_buffer_.value + registers_.x.full;
					data_address_increment_mask_ = 0xff'ff'ff;
				continue;

				case OperationConstructAbsoluteXRead:
				case OperationConstructAbsoluteX:
					data_address_ = instruction_buffer_.value + registers_.x.full + registers_.data_bank;
					incorrect_data_address_ = ((data_address_ & 0x00ff) | (instruction_buffer_.value & 0xff00)) + registers_.data_bank;

					// If the incorrect address isn't actually incorrect, skip its usage.
					if(operation == OperationConstructAbsoluteXRead && data_address_ == incorrect_data_address_) {
						++next_op_;
					}
					data_address_increment_mask_ = 0xff'ff'ff;
				continue;

				case OperationConstructAbsoluteYRead:
				case OperationConstructAbsoluteY:
					data_address_ = instruction_buffer_.value + registers_.y.full + registers_.data_bank;
					incorrect_data_address_ = (data_address_ & 0xff) + (instruction_buffer_.value & 0xff00) + registers_.data_bank;

					// If the incorrect address isn't actually incorrect, skip its usage.
					if(operation == OperationConstructAbsoluteYRead && data_address_ == incorrect_data_address_) {
						++next_op_;
					}
					data_address_increment_mask_ = 0xff'ff'ff;
				continue;

				case OperationConstructDirect:
					data_address_ = (registers_.direct + instruction_buffer_.value) & 0xffff;
					data_address_increment_mask_ = 0x00'ff'ff;
					if(!(registers_.direct&0xff)) {
						// If the low byte is 0 and this is emulation mode, incrementing
						// is restricted to the low byte.
						data_address_increment_mask_ = registers_.e_masks[1];
						++next_op_;
					}
				continue;

				case OperationConstructDirectLong:
					data_address_ = (registers_.direct + instruction_buffer_.value) & 0xffff;
					data_address_increment_mask_ = 0x00'ff'ff;
					if(!(registers_.direct&0xff)) {
						++next_op_;
					}
				continue;

				case OperationConstructDirectIndirect:
					data_address_ = registers_.data_bank + data_buffer_.value;
					data_address_increment_mask_ = 0xff'ff'ff;
					data_buffer_.clear();
				continue;

				case OperationConstructDirectIndexedIndirect:
					data_address_ = registers_.data_bank + ((
						((registers_.direct + registers_.x.full + instruction_buffer_.value) & registers_.e_masks[1]) +
						(registers_.direct & registers_.e_masks[0])
					) & 0xffff);
					data_address_increment_mask_ = 0x00'ff'ff;

					if(!(registers_.direct&0xff)) {
						++next_op_;
					}
				continue;

				case OperationConstructDirectIndirectIndexedLong:
					data_address_ = registers_.y.full + data_buffer_.value;
					data_address_increment_mask_ = 0xff'ff'ff;
					data_buffer_.clear();
				continue;

				case OperationConstructDirectIndirectLong:
					data_address_ = data_buffer_.value;
					data_address_increment_mask_ = 0xff'ff'ff;
					data_buffer_.clear();
				continue;

				// TODO: confirm incorrect_data_address_ below.

				case OperationConstructDirectX:
					data_address_ = (
						(registers_.direct & registers_.e_masks[0]) +
						((instruction_buffer_.value + registers_.direct + registers_.x.full) & registers_.e_masks[1])
					) & 0xffff;
					data_address_increment_mask_ = 0x00'ff'ff;

					incorrect_data_address_ = (registers_.direct & 0xff00) + (data_address_ & 0x00ff);
					if(!(registers_.direct&0xff)) {
						++next_op_;
					}
				continue;

				case OperationConstructDirectY:
					data_address_ = (
						(registers_.direct & registers_.e_masks[0]) +
						((instruction_buffer_.value + registers_.direct + registers_.y.full) & registers_.e_masks[1])
					) & 0xffff;
					data_address_increment_mask_ = 0x00'ff'ff;

					incorrect_data_address_ = (registers_.direct & 0xff00) + (data_address_ & 0x00ff);
					if(!(registers_.direct&0xff)) {
						++next_op_;
					}
				continue;

				case OperationConstructStackRelative:
					data_address_ = (registers_.s.full + instruction_buffer_.value) & 0xffff;
					data_address_increment_mask_ = 0x00'ff'ff;
				continue;

				case OperationConstructStackRelativeIndexedIndirect:
					data_address_ = registers_.data_bank + data_buffer_.value + registers_.y.full;
					data_address_increment_mask_ = 0xff'ff'ff;
					data_buffer_.clear();
				continue;

				case OperationConstructPER:
					data_buffer_.value = instruction_buffer_.value + registers_.pc;
					data_buffer_.size = 2;
				continue;

				case OperationPrepareException:
					data_buffer_.value = uint32_t((registers_.pc << 8) | get_flags());
					if(registers_.emulation_flag) {
						if(!exception_is_interrupt_) data_buffer_.value |= Flag::Break;
						data_buffer_.size = 3;
						registers_.data_bank = 0;
						++next_op_;
					} else {
						data_buffer_.value |= registers_.program_bank << 8;	// The PBR is always held such that
																			// PBR+PC produces a 24-bit address;
																			// therefore a shift by 8 is correct
																			// here — it matches the shift applied
																			// to .pc above.
						data_buffer_.size = 4;
					}

					registers_.program_bank = 0;
					registers_.flags.inverse_interrupt = 0;
					registers_.flags.decimal = 0;
				continue;

				case OperationPickExceptionVector:
					// Priority for abort and reset here is a guess.

					if(pending_exceptions_ & (Reset | PowerOn)) {
						pending_exceptions_ &= ~(Reset | PowerOn);
						data_address_ = 0xfffc;
						set_reset_state();
						continue;
					}

					if(pending_exceptions_ & Abort) {
						// Special case: restore registers from start of instruction.
						registers_ = abort_registers_copy_;

						pending_exceptions_ &= ~Abort;
						data_address_ = registers_.emulation_flag ? 0xfff8 : 0xffe8;
						continue;
					}

					if(pending_exceptions_ & NMI) {
						pending_exceptions_ &= ~NMI;
						data_address_ = registers_.emulation_flag ? 0xfffa : 0xffea;
						continue;
					}

					// Last chance saloon for the interrupt process.
					if(exception_is_interrupt_) {
						data_address_ = registers_.emulation_flag ? 0xfffe : 0xffee;
						continue;
					}

					// ... then this must be a BRK or COP that is being treated as such.
					assert((active_instruction_ == instructions) || (active_instruction_ == &instructions[0x02]));

					// Test for BRK, given that it has opcode 00.
					if(active_instruction_ == instructions) {
						data_address_ = registers_.emulation_flag ? 0xfffe : 0xffe6;
					} else {
						// Implicitly: COP.
						data_address_ = registers_.emulation_flag ? 0xfff4 : 0xffe4;
					}
				continue;

				//
				// Performance.
				//

#define LDA(src) 		registers_.a.full = (registers_.a.full & registers_.m_masks[0]) | (src & registers_.m_masks[1])
#define LDXY(dest, src)	dest = (src) & registers_.x_mask

				case OperationPerform:
					switch(active_instruction_->operation) {

						//
						// Loads, stores and transfers (and NOP, and XBA).
						//

						case LDA:
							LDA(data_buffer_.value);
							registers_.flags.set_nz(registers_.a.full, registers_.m_shift);
						break;

						case LDX:
							LDXY(registers_.x, data_buffer_.value);
							registers_.flags.set_nz(registers_.x.full, registers_.x_shift);
						break;

						case LDY:
							LDXY(registers_.y, data_buffer_.value);
							registers_.flags.set_nz(registers_.y.full, registers_.x_shift);
						break;

						case PLB:
							registers_.data_bank = (data_buffer_.value & 0xff) << 16;
							registers_.flags.set_nz(uint8_t(data_buffer_.value));
						break;

						case PLD:
							registers_.direct = uint16_t(data_buffer_.value);
							registers_.flags.set_nz(uint16_t(data_buffer_.value), 8);
						break;

						case PLP:
							set_flags(uint8_t(data_buffer_.value));
						break;

						case STA:
							data_buffer_.value = registers_.a.full & registers_.m_masks[1];
							data_buffer_.size = 2 - m_flag();
						break;

						case STZ:
							data_buffer_.value = 0;
							data_buffer_.size = 2 - m_flag();
						break;

						case STX:
							data_buffer_.value = registers_.x.full;
							data_buffer_.size = 2 - x_flag();
						break;

						case STY:
							data_buffer_.value = registers_.y.full;
							data_buffer_.size = 2 - x_flag();
						break;

						case PHB:
							data_buffer_.value = registers_.data_bank >> 16;
							data_buffer_.size = 1;
						break;

						case PHK:
							data_buffer_.value = registers_.program_bank >> 16;
							data_buffer_.size = 1;
						break;

						case PHD:
							data_buffer_.value = registers_.direct;
							data_buffer_.size = 2;
						break;

						case PHP:
							data_buffer_.value = get_flags();
							data_buffer_.size = 1;

							if(registers_.emulation_flag) {
								// On the 6502, the break flag is set during a PHP.
								data_buffer_.value |= Flag::Break;
							}
						break;

						case NOP:						break;
						case WDM:	++registers_.pc;	break;


						// The below attempt to obey the 8/16-bit mixed transfer rules
						// as documented in https://softpixel.com/~cwright/sianse/docs/65816NFO.HTM
						// (and make reasonable guesses as to the N flag).

						case TXS:
							registers_.s = registers_.x.full;
						break;

						case TSX:
							LDXY(registers_.x, registers_.s.full);
							registers_.flags.set_nz(registers_.x.full, registers_.x_shift);
						break;

						case TXY:
							LDXY(registers_.y, registers_.x.full);
							registers_.flags.set_nz(registers_.y.full, registers_.x_shift);
						break;

						case TYX:
							LDXY(registers_.x, registers_.y.full);
							registers_.flags.set_nz(registers_.x.full, registers_.x_shift);
						break;

						case TAX:
							LDXY(registers_.x, registers_.a.full);
							registers_.flags.set_nz(registers_.x.full, registers_.x_shift);
						break;

						case TAY:
							LDXY(registers_.y, registers_.a.full);
							registers_.flags.set_nz(registers_.y.full, registers_.x_shift);
						break;

						case TXA:
							LDA(registers_.x.full);
							registers_.flags.set_nz(registers_.a.full, registers_.m_shift);
						break;

						case TYA:
							LDA(registers_.y.full);
							registers_.flags.set_nz(registers_.a.full, registers_.m_shift);
						break;

						case TCD:
							registers_.direct = registers_.a.full;
							registers_.flags.set_nz(registers_.a.full, 8);
						break;

						case TDC:
							registers_.a.full = registers_.direct;
							registers_.flags.set_nz(registers_.a.full, 8);
						break;

						case TCS:
							registers_.s.full = registers_.a.full;
							// No need to worry about byte masking here; for the stack it's handled as the emulation runs.
						break;

						case TSC:
							registers_.a.full = stack_address();
							registers_.flags.set_nz(registers_.a.full, 8);
						break;

						case XBA: {
							const uint8_t a_low = registers_.a.halves.low;
							registers_.a.halves.low = registers_.a.halves.high;
							registers_.a.halves.high = a_low;
							registers_.flags.set_nz(registers_.a.halves.low);
						} break;

						//
						// Jumps and returns.
						//

						case JML:
							registers_.program_bank = data_buffer_.value & 0xff0000;
							registers_.pc = uint16_t(data_buffer_.value);
						break;

						case JMP:
							registers_.pc = uint16_t(instruction_buffer_.value);
						break;

						case JMPind:
							registers_.pc = uint16_t(data_buffer_.value);
						break;

						case RTL:
							registers_.program_bank = data_buffer_.value & 0xff0000;
						[[fallthrough]];

						case RTS:
							registers_.pc = uint16_t(data_buffer_.value + 1);
						break;

						case JSL:
							registers_.program_bank = instruction_buffer_.value & 0xff0000;
						[[fallthrough]];

						case JSR:
							data_buffer_.value = registers_.pc;
							data_buffer_.size = 2;

							registers_.pc = uint16_t(instruction_buffer_.value);
						break;

						case RTI:
							registers_.pc = uint16_t(data_buffer_.value >> 8);
							set_flags(uint8_t(data_buffer_.value));

							if(!registers_.emulation_flag) {
								registers_.program_bank = (data_buffer_.value & 0xff000000) >> 8;
							}
						break;

						//
						// Block moves.
						//

						case MVP:
							registers_.data_bank = (instruction_buffer_.value & 0xff) << 16;
							LDXY(registers_.x.full, registers_.x.full - 1);
							LDXY(registers_.y.full, registers_.y.full - 1);
							if(registers_.a.full) registers_.pc -= 3;
							--registers_.a.full;
						break;

						case MVN:
							registers_.data_bank = (instruction_buffer_.value & 0xff) << 16;
							LDXY(registers_.x.full, registers_.x.full + 1);
							LDXY(registers_.y.full, registers_.y.full + 1);
							if(registers_.a.full) registers_.pc -= 3;
							--registers_.a.full;
						break;

						//
						// Flag manipulation.
						//

						case CLC: registers_.flags.carry = 0;							break;
						case CLI: registers_.flags.inverse_interrupt = Flag::Interrupt;	break;
						case CLV: registers_.flags.overflow = 0;						break;
						case CLD: registers_.flags.decimal = 0;							break;

						case SEC: registers_.flags.carry = Flag::Carry;					break;
						case SEI: registers_.flags.inverse_interrupt = 0;				break;
						case SED: registers_.flags.decimal = Flag::Decimal;				break;

						case REP:
							set_flags(uint8_t(get_flags() &~ instruction_buffer_.value));
						break;

						case SEP:
							set_flags(uint8_t(get_flags() | instruction_buffer_.value));
						break;

						case XCE: {
							const bool old_emulation_flag = registers_.emulation_flag;
							set_emulation_mode(registers_.flags.carry);
							registers_.flags.carry = old_emulation_flag;
						} break;

						//
						// Increments and decrements.
						//

						case INC:
							++data_buffer_.value;
							registers_.flags.set_nz(uint16_t(data_buffer_.value), registers_.m_shift);
						break;;

						case DEC:
							--data_buffer_.value;
							registers_.flags.set_nz(uint16_t(data_buffer_.value), registers_.m_shift);
						break;

						case INX:
							LDXY(registers_.x.full, registers_.x.full + 1);
							registers_.flags.set_nz(registers_.x.full, registers_.x_shift);
						break;

						case DEX:
							LDXY(registers_.x.full, registers_.x.full - 1);
							registers_.flags.set_nz(registers_.x.full, registers_.x_shift);
						break;

						case INY:
							LDXY(registers_.y.full, registers_.y.full + 1);
							registers_.flags.set_nz(registers_.y.full, registers_.x_shift);
						break;

						case DEY:
							LDXY(registers_.y.full, registers_.y.full - 1);
							registers_.flags.set_nz(registers_.y.full, registers_.x_shift);
						break;

						//
						// Bitwise operations.
						//

						case AND:
							registers_.a.full &= data_buffer_.value | registers_.m_masks[0];
							registers_.flags.set_nz(registers_.a.full, registers_.m_shift);
						break;

						case EOR:
							registers_.a.full ^= data_buffer_.value;
							registers_.flags.set_nz(registers_.a.full, registers_.m_shift);
						break;

						case ORA:
							registers_.a.full |= data_buffer_.value;
							registers_.flags.set_nz(registers_.a.full, registers_.m_shift);
						break;

						case BIT:
							registers_.flags.set_n(uint16_t(data_buffer_.value), registers_.m_shift);
							registers_.flags.set_z(uint16_t(data_buffer_.value & registers_.a.full), registers_.m_shift);
							registers_.flags.overflow = data_buffer_.value & Flag::Overflow;
						break;

						case BITimm:
							registers_.flags.set_z(data_buffer_.value & registers_.a.full, registers_.m_shift);
						break;

						case TRB:
							registers_.flags.set_z(data_buffer_.value & registers_.a.full, registers_.m_shift);
							data_buffer_.value &= ~registers_.a.full;
						break;

						case TSB:
							registers_.flags.set_z(data_buffer_.value & registers_.a.full, registers_.m_shift);
							data_buffer_.value |= registers_.a.full;
						break;

						//
						// Branches.
						//

#define BRA(condition)	\
	if(!(condition)) {	\
		next_op_ += 3;	\
	} else {			\
		data_buffer_.size = 2;	\
		data_buffer_.value = uint32_t(registers_.pc + int8_t(instruction_buffer_.value));	\
																							\
		if((registers_.pc & 0xff00) == (instruction_buffer_.value & 0xff00)) {				\
			++next_op_;																		\
		}																					\
	}

						case BPL: BRA(!(registers_.flags.negative_result&0x80));	break;
						case BMI: BRA(registers_.flags.negative_result&0x80);		break;
						case BVC: BRA(!registers_.flags.overflow);					break;
						case BVS: BRA(registers_.flags.overflow);					break;
						case BCC: BRA(!registers_.flags.carry);						break;
						case BCS: BRA(registers_.flags.carry);						break;
						case BNE: BRA(registers_.flags.zero_result);				break;
						case BEQ: BRA(!registers_.flags.zero_result);				break;
						case BRA: BRA(true);										break;

#undef BRA

						case BRL:
							registers_.pc += int16_t(instruction_buffer_.value);
						break;

						//
						// Shifts and rolls.
						//

						case ASL:
							registers_.flags.carry = uint8_t(data_buffer_.value >> (7 + registers_.m_shift));
							data_buffer_.value <<= 1;
							registers_.flags.set_nz(uint16_t(data_buffer_.value), registers_.m_shift);
						break;

						case LSR:
							registers_.flags.carry = uint8_t(data_buffer_.value & 1);
							data_buffer_.value >>= 1;
							registers_.flags.set_nz(uint16_t(data_buffer_.value), registers_.m_shift);
						break;

						case ROL:
							data_buffer_.value = (data_buffer_.value << 1) | registers_.flags.carry;
							registers_.flags.carry = uint8_t(data_buffer_.value >> (8 + registers_.m_shift));
							registers_.flags.set_nz(uint16_t(data_buffer_.value), registers_.m_shift);
						break;

						case ROR: {
							const uint8_t next_carry = data_buffer_.value & 1;
							data_buffer_.value = (data_buffer_.value >> 1) | (uint32_t(registers_.flags.carry) << (7 + registers_.m_shift));
							registers_.flags.carry = next_carry;
							registers_.flags.set_nz(uint16_t(data_buffer_.value), registers_.m_shift);
						} break;

						//
						// Arithmetic.
						//

#define cp(v, shift, mask)	{\
	const uint32_t temp32 = (v.full & mask) - (data_buffer_.value & mask);	\
	registers_.flags.set_nz(uint16_t(temp32), shift);	\
	registers_.flags.carry = ((~temp32) >> (8 + shift))&1;	\
}

						case CMP:	cp(registers_.a, registers_.m_shift, registers_.m_masks[1]);	break;
						case CPX:	cp(registers_.x, registers_.x_shift, registers_.x_mask);		break;
						case CPY:	cp(registers_.y, registers_.x_shift, registers_.x_mask);		break;

#undef cp

						// As implemented below, both ADC and SBC apply the 6502 test for overflow (i.e. based
						// on intermediate results) rather than the 65C02 (i.e. based on the final result).
						// This tracks the online tests I found, which hail from Nintendo world. So I'm currently
						// unclear whether this is correct or merely a figment of Nintendo's custom chip.

						case SBC:
							if(registers_.flags.decimal) {
								// I've yet to manage to find a rational way to map this to an ADC,
								// hence the yucky repetition of code here.
								const uint16_t a = registers_.a.full & registers_.m_masks[1];
								unsigned int result = 0;
								unsigned int borrow = registers_.flags.carry ^ 1;
								const uint16_t decimal_result = uint16_t(a - data_buffer_.value - borrow);

#define nibble(mask, adjustment, carry)								\
	result += (a & mask) - (data_buffer_.value & mask) - borrow;	\
	if(result > mask) result -= adjustment;							\
	borrow = (result > mask) ? carry : 0;							\
	result &= (carry - 1);

								nibble(0x000f, 0x0006, 0x00010);
								nibble(0x00f0, 0x0060, 0x00100);
								nibble(0x0f00, 0x0600, 0x01000);
								nibble(0xf000, 0x6000, 0x10000);

#undef nibble

								registers_.flags.overflow = (( (decimal_result ^ a) & (~decimal_result ^ data_buffer_.value) ) >> (1 + registers_.m_shift))&0x40;
								registers_.flags.set_nz(uint16_t(result), registers_.m_shift);
								registers_.flags.carry = ((borrow >> 16)&1)^1;
								LDA(result);

								break;
							}

							data_buffer_.value = ~data_buffer_.value & registers_.m_masks[1];
						[[fallthrough]];

						case ADC: {
							int result;
							const uint16_t a = registers_.a.full & registers_.m_masks[1];

							if(registers_.flags.decimal) {
								uint16_t partials = 0;
								result = registers_.flags.carry;

#define nibble(mask, limit, adjustment, carry)			\
	result += (a & mask) + (data_buffer_.value & mask);	\
	partials += result & mask;							\
	if(result >= limit) result = ((result + (adjustment)) & (carry - 1)) + carry;

								nibble(0x000f, 0x000a, 0x0006, 0x00010);
								nibble(0x00f0, 0x00a0, 0x0060, 0x00100);
								nibble(0x0f00, 0x0a00, 0x0600, 0x01000);
								nibble(0xf000, 0xa000, 0x6000, 0x10000);

#undef nibble

								registers_.flags.overflow = (( (partials ^ registers_.a.full) & (partials ^ data_buffer_.value) )  >> (1 + registers_.m_shift))&0x40;

							} else {
								result = int(a + data_buffer_.value + registers_.flags.carry);
								registers_.flags.overflow = (( (uint16_t(result) ^ registers_.a.full) & (uint16_t(result) ^ data_buffer_.value) ) >> (1 + registers_.m_shift))&0x40;
							}

							registers_.flags.set_nz(uint16_t(result), registers_.m_shift);
							registers_.flags.carry = (result >> (8 + registers_.m_shift))&1;
							LDA(result);
						} break;

						//
						// STP and WAI
						//

						case STP:
							required_exceptions_ = Reset;
						break;

						case WAI:
							required_exceptions_ = Reset | IRQ | NMI;
						break;
					}
				continue;
			}

			// Store a selection as to the exceptions, if any, that would be honoured after this cycle if the
			// next thing is a MoveToNextProgram.
			selected_exceptions_ = pending_exceptions_ & (registers_.flags.inverse_interrupt | PowerOn | Reset | NMI);
			number_of_cycles -= bus_handler_.perform_bus_operation(bus_operation_, static_cast<typename BusHandler::AddressType>(bus_address_), bus_value_);
		}
	}

#undef LDA
#undef LDXY
#undef read
#undef write
#undef bus_operation
#undef x
#undef y
#undef m_flag
#undef x_flag
#undef stack_address

	cycles_left_to_run_ = number_of_cycles;
	bus_handler_.flush();
}

void ProcessorBase::set_power_on(bool active) {
	if(active) {
		pending_exceptions_ |= PowerOn;
	} else {
		pending_exceptions_ &= ~PowerOn;
		selected_exceptions_ &= ~PowerOn;
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

void ProcessorBase::set_abort_line(bool active) {
	// Take a copy of register state now to restore at the beginning of the exception
	// if abort has gone active, preparing to regress the program counter.
	if(active) {
		pending_exceptions_ |= Abort;
		abort_registers_copy_ = registers_;
		abort_registers_copy_.pc = last_operation_pc_;
	} else {
		pending_exceptions_ &= ~Abort;
	}
}

template <typename BusHandler, bool uses_ready_line> void Processor<BusHandler, uses_ready_line>::set_ready_line(bool active) {
	assert(uses_ready_line);
	ready_line_ = active;
}

// The 65816 can't jam.
bool ProcessorBase::is_jammed() const { return false; }

bool ProcessorBase::get_is_resetting() const {
	return pending_exceptions_ & (Reset | PowerOn);
}

int ProcessorBase::get_extended_bus_output() {
	return
		(memory_lock_ ? ExtendedBusOutput::MemoryLock : 0) |
		(registers_.mx_flags[0] ? ExtendedBusOutput::MemorySize : 0) |
		(registers_.mx_flags[1] ? ExtendedBusOutput::IndexSize : 0) |
		(registers_.emulation_flag ? ExtendedBusOutput::Emulation : 0);
}
