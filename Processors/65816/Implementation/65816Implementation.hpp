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
				printf("%02x\n", instruction_buffer_.value);
				active_instruction_ = &instructions[instruction_offset_ + instruction_buffer_.value];

				const auto size_flag = mx_flags_[active_instruction_->size_field];
				next_op_ = &micro_ops_[active_instruction_->program_offsets[size_flag]];
				instruction_buffer_.clear();
			} continue;

			//
			// PC fetches.
			//

			case CycleFetchIncrementPC:
			case CycleFetchOpcode:
				bus_address = pc_ | program_bank_;
				bus_value = instruction_buffer_.next();
				bus_operation = (operation == CycleFetchOpcode) ? MOS6502Esque::ReadOpcode : MOS6502Esque::Read;
				// TODO: split this action when I'm happy that my route to bus accesses is settled, to avoid repeating the conditional
				// embedded into the `switch`.
				++pc_;
			break;

			case CycleFetchPC:
				bus_address = pc_ | program_bank_;
				bus_value = &throwaway;
				bus_operation = MOS6502Esque::Read;
			break;

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

					default:
						assert(false);
				}
			break;

			default:
				assert(false);
		}

		number_of_cycles -= bus_handler_.perform_bus_operation(bus_operation, bus_address, bus_value);
	}

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
