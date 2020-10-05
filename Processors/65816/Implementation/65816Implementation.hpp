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
				// A VERY TEMPORARY piece of logging.
				printf("[%04x] %02x\n", pc_ - 1, instruction_buffer_.value);
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
			break;

			case OperationCopyInstructionToData:
				data_buffer_ = instruction_buffer_;
			break;

			//
			// Address construction.
			//

			case OperationConstructAbsolute:
				data_address_ = instruction_buffer_.value | data_bank_;
			break;

			case OperationConstructAbsoluteIndexedIndirect:
				data_address_ = (instruction_buffer_.value + x()) & 0xffff;
			break;

			case OperationConstructAbsoluteLongX:
				data_address_ = instruction_buffer_.value + x();
			break;

			case OperationConstructAbsoluteXRead:
			case OperationConstructAbsoluteX:
				data_address_ = ((instruction_buffer_.value + x()) & 0xffff) | data_bank_;
				incorrect_data_address_ = (data_address_ & 0xff) | (instruction_buffer_.value & 0xff00) | data_bank_;

				// If the incorrect address isn't actually incorrect, skip its usage.
				if(operation == OperationConstructAbsoluteXRead && data_address_ == incorrect_data_address_) {
					++next_op_;
				}
			break;

			case OperationConstructAbsoluteYRead:
			case OperationConstructAbsoluteY:
				data_address_ = ((instruction_buffer_.value + y()) & 0xffff) | data_bank_;
				incorrect_data_address_ = (data_address_ & 0xff) | (instruction_buffer_.value & 0xff00) | data_bank_;

				// If the incorrect address isn't actually incorrect, skip its usage.
				if(operation == OperationConstructAbsoluteYRead && data_address_ == incorrect_data_address_) {
					++next_op_;
				}
			break;

			case OperationConstructDirect:
				data_address_ = (direct_ + instruction_buffer_.value) & 0xffff;
				if(!(direct_&0xff)) {
					++next_op_;
				}
			break;

			case OperationConstructDirectIndexedIndirect:
				data_address_ = data_bank_ + (direct_ + x() + instruction_buffer_.value) & 0xffff;
				if(!(direct_&0xff)) {
					++next_op_;
				}
			break;

			case OperationConstructDirectIndirect:
				data_address_ = data_bank_ + (direct_ + instruction_buffer_.value) & 0xffff;
				if(!(direct_&0xff)) {
					++next_op_;
				}
			break;

			//
			// Performance.
			//

			case OperationPerform:
				switch(active_instruction_->operation) {

					//
					// Flag manipulation.
					//

					case CLD:
						decimal_flag_ = 0;
					break;

					//
					// Loads, stores and transfers
					//

#define LD(dest, src, masks) dest.full = (dest.full & masks[0]) | (src & masks[1])

					case LDA:
						LD(a_, data_buffer_.value, m_masks_);
					break;

					case LDX:
						LD(x_, data_buffer_.value, x_masks_);
					break;

					case LDY:
						LD(y_, data_buffer_.value, x_masks_);
					break;

					case TXS:
						// TODO: does this transfer in full when in 8-bit index mode?
						LD(s_, x_.full, x_masks_);
					break;

#undef LD

					case STA:
						data_buffer_.value = a_.full & m_masks_[1];
						data_buffer_.size = 2 - mx_flags_[0];
					break;

					//
					// Jumps.
					//

					case JML:
						program_bank_ = instruction_buffer_.value & 0xff0000;
						pc_ = instruction_buffer_.value & 0xffff;
					break;

					case JSL:
						program_bank_ = instruction_buffer_.value & 0xff0000;
						instruction_buffer_.size = 2;
					[[fallthrough]];

					case JSR: {
						const uint16_t old_pc = pc_;
						pc_ = instruction_buffer_.value;
						instruction_buffer_.value = old_pc;
					} break;

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

					default:
						assert(false);
				}
			break;

			default:
				assert(false);
		}

		number_of_cycles -= bus_handler_.perform_bus_operation(bus_operation, bus_address, bus_value);
	}

#undef read
#undef write
#undef bus_operation
#undef x
#undef y

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
