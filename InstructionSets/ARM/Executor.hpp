//
//  Executor.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 01/03/2024.
//  Copyright © 2024 Thomas Harte. All rights reserved.
//

#pragma once

#include "BarrelShifter.hpp"
#include "OperationMapper.hpp"
#include "Registers.hpp"
#include "../../Numeric/Carry.hpp"

namespace InstructionSet::ARM {

/// A class compatible with the @c OperationMapper definition of a scheduler which applies all actions
/// immediately, updating either a set of @c Registers or using the templated @c MemoryT to access
/// memory. No hooks are currently provided for applying realistic timing.
template <Model model, typename MemoryT>
struct Executor {
	bool should_schedule(Condition condition) {
		return registers_.test(condition);
	}

	template <bool allow_register, bool set_carry, typename FieldsT>
	uint32_t decode_shift(FieldsT fields, uint32_t &rotate_carry, uint32_t pc_offset) {
		// "When R15 appears in the Rm position it will give the value of the PC together
		// with the PSR flags to the barrel shifter. ...
		//
		// If the shift amount is specified in the instruction, the PC will be 8 bytes ahead.
		// If a register is used to specify the shift amount, the PC will be ... 12 bytes ahead
		// when used as Rn or Rm."
		uint32_t operand2;
		if(fields.operand2() == 15) {
			operand2 = registers_.pc_status(pc_offset);
		} else {
			operand2 = registers_.active[fields.operand2()];
		}

		uint32_t shift_amount;
		if constexpr (allow_register) {
			if(fields.shift_count_is_register()) {
				// "When R15 appears in either of the Rn or Rs positions it will give the value
				// of the PC alone, with the PSR bits replaced by zeroes. ...
				//
				// If a register is used to specify the shift amount, the
				// PC will be 8 bytes ahead when used as Rs."
				shift_amount =
					fields.shift_register() == 15 ?
						registers_.pc(4) :
						registers_.active[fields.shift_register()];

				// A register shift amount of 0 has a different meaning than an in-instruction
				// shift amount of 0.
				if(!shift_amount) {
					return operand2;
				}
			} else {
				shift_amount = fields.shift_amount();
			}
		} else {
			shift_amount = fields.shift_amount();
		}

		shift<set_carry>(fields.shift_type(), operand2, shift_amount, rotate_carry);
		return operand2;
	}

	template <Flags f> void perform(DataProcessing fields) {
		constexpr DataProcessingFlags flags(f);
		const bool shift_by_register = !flags.operand2_is_immediate() && fields.shift_count_is_register();

		// Write a raw result into the PC proxy if the target is R15; it'll be stored properly later.
		uint32_t pc_proxy = 0;
		auto &destination = fields.destination() == 15 ? pc_proxy : registers_.active[fields.destination()];

		// "When R15 appears in either of the Rn or Rs positions it will give the value
		// of the PC alone, with the PSR bits replaced by zeroes. ...
		//
		// If the shift amount is specified in the instruction, the PC will be 8 bytes ahead.
		// If a register is used to specify the shift amount, the PC will be ... 12 bytes ahead
		// when used as Rn or Rm."
		const uint32_t operand1 =
			(fields.operand1() == 15) ?
				registers_.pc(shift_by_register ? 8 : 4) :
				registers_.active[fields.operand1()];

		uint32_t operand2;
		uint32_t rotate_carry = registers_.c();

		// Populate carry from the shift only if it'll be used.
		constexpr bool shift_sets_carry = is_logical(flags.operation()) && flags.set_condition_codes();

		// Get operand 2.
		if constexpr (flags.operand2_is_immediate()) {
			operand2 = fields.immediate();
			if(fields.rotate()) {
				shift<ShiftType::RotateRight, shift_sets_carry>(operand2, fields.rotate(), rotate_carry);
			}
		} else {
			operand2 = decode_shift<true, shift_sets_carry>(fields, rotate_carry, shift_by_register ? 8 : 4);
		}

		// Perform the data processing operation.
		uint32_t conditions = 0;
		switch(flags.operation()) {
			// Logical operations.
			case DataProcessingOperation::AND:	conditions = destination = operand1 & operand2;		break;
			case DataProcessingOperation::EOR:	conditions = destination = operand1 ^ operand2;		break;
			case DataProcessingOperation::ORR:	conditions = destination = operand1 | operand2;		break;
			case DataProcessingOperation::BIC:	conditions = destination = operand1 & ~operand2;	break;

			case DataProcessingOperation::MOV:	conditions = destination = operand2;	break;
			case DataProcessingOperation::MVN:	conditions = destination = ~operand2;	break;

			case DataProcessingOperation::TST:	conditions = operand1 & operand2;	break;
			case DataProcessingOperation::TEQ:	conditions = operand1 ^ operand2;	break;

			case DataProcessingOperation::ADD:
			case DataProcessingOperation::ADC:
			case DataProcessingOperation::CMN:
				conditions = operand1 + operand2;

				if constexpr (flags.operation() == DataProcessingOperation::ADC) {
					conditions += registers_.c();
				}

				if constexpr (flags.set_condition_codes()) {
					registers_.set_c(Numeric::carried_out<true, 31>(operand1, operand2, conditions));
					registers_.set_v(Numeric::overflow<true>(operand1, operand2, conditions));
				}

				if constexpr (!is_comparison(flags.operation())) {
					destination = conditions;
				}
			break;

			case DataProcessingOperation::SUB:
			case DataProcessingOperation::SBC:
			case DataProcessingOperation::CMP:
				conditions = operand1 - operand2;

				if constexpr (flags.operation() == DataProcessingOperation::SBC) {
					conditions -= registers_.c();
				}

				if constexpr (flags.set_condition_codes()) {
					registers_.set_c(Numeric::carried_out<false, 31>(operand1, operand2, conditions));
					registers_.set_v(Numeric::overflow<false>(operand1, operand2, conditions));
				}

				if constexpr (!is_comparison(flags.operation())) {
					destination = conditions;
				}
			break;

			case DataProcessingOperation::RSB:
			case DataProcessingOperation::RSC:
				conditions = operand2 - operand1;

				if constexpr (flags.operation() == DataProcessingOperation::RSC) {
					conditions -= registers_.c();
				}

				if constexpr (flags.set_condition_codes()) {
					registers_.set_c(Numeric::carried_out<false, 31>(operand2, operand1, conditions));
					registers_.set_v(Numeric::overflow<false>(operand2, operand1, conditions));
				}

				destination = conditions;
			break;
		}

		if constexpr (flags.set_condition_codes()) {
			// "When Rd is a register other than R15, the condition code flags in the PSR may be
			// updated from the ALU flags as described above. When Rd is R15 and the S flag in
			// the instruction is set, the PSR is overwritten by the corresponding ALU result.
			//
			// ... if the instruction is of a type which does not normally produce a result
			// (CMP, CMN, TST, TEQ) but Rd is R15 and the S bit is set, the result will be used in
			// this case to update those PSR flags which are not protected by virtue of the
			// processor mode."

			if(fields.destination() == 15) {
				if constexpr (is_comparison(flags.operation())) {
					registers_.set_status(pc_proxy);
				} else {
					registers_.set_status(pc_proxy);
					registers_.set_pc(pc_proxy);
				}
			} else {
				// Set N and Z in a unified way.
				registers_.set_nz(conditions);

				// Set C from the barrel shifter if applicable.
				if constexpr (shift_sets_carry) {
					registers_.set_c(rotate_carry);
				}
			}
		} else {
			// "If the S flag is clear when Rd is R15, only the 24 PC bits of R15 will be written."
			if(fields.destination() == 15) {
				registers_.set_pc(pc_proxy);
			}
		}
	}

	template <Flags f> void perform(Multiply fields) {
		constexpr MultiplyFlags flags(f);

		// R15 rules:
		//
		//	* Rs: no PSR, 8 bytes ahead;
		//	* Rn: with PSR, 8 bytes ahead;
		//	* Rm: with PSR, 12 bytes ahead.

		const uint32_t multiplicand = fields.multiplicand() == 15 ? registers_.pc(4) : registers_.active[fields.multiplicand()];
		const uint32_t multiplier = fields.multiplier() == 15 ? registers_.pc_status(4) : registers_.active[fields.multiplier()];
		const uint32_t accumulator =
			flags.operation() == MultiplyFlags::Operation::MUL ? 0 :
				(fields.multiplicand() == 15 ? registers_.pc_status(8) : registers_.active[fields.accumulator()]);

		const uint32_t result = multiplicand * multiplier + accumulator;

		if constexpr (flags.set_condition_codes()) {
			registers_.set_nz(result);
			// V is unaffected; C is undefined.
		}

		if(fields.destination() != 15) {
			registers_.active[fields.destination()] = result;
		}
	}

	template <Flags f> void perform(Branch branch) {
		constexpr BranchFlags flags(f);

		if constexpr (flags.operation() == BranchFlags::Operation::BL) {
			registers_.active[14] = registers_.pc(0);
		}
		registers_.set_pc(registers_.pc(4) + branch.offset());
	}

	template <Flags f> void perform(SingleDataTransfer transfer) {
		constexpr SingleDataTransferFlags flags(f);

		// Calculate offset.
		uint32_t offset;
		if constexpr (flags.offset_is_immediate()) {
			offset = transfer.immediate();
		} else {
			// The 8 shift control bits are described in 6.2.3, but
			// the register specified shift amounts are not available
			// in this instruction class.
			uint32_t carry = registers_.c();
			offset = decode_shift<false, false>(transfer, carry, 4);
		}

		// Obtain base address.
		uint32_t address =
			transfer.base() == 15 ?
				registers_.pc(4) :
				registers_.active[transfer.base()];

		// Determine what the address will be after offsetting.
		uint32_t offsetted_address = address;
		if constexpr (flags.add_offset()) {
			offsetted_address += offset;
		} else {
			offsetted_address -= offset;
		}

		// If preindexing, apply now.
		if constexpr (flags.pre_index()) {
			address = offsetted_address;
		}

		// Check for an address exception.
		if(address >= (1 << 26)) {
			registers_.exception<Registers::Exception::Address>();
			return;
		}

		constexpr bool trans = !flags.pre_index() && flags.write_back_address();
		if constexpr (flags.operation() == SingleDataTransferFlags::Operation::STR) {
			const uint32_t source =
				transfer.source() == 15 ?
					registers_.pc_status(8) :
					registers_.active[transfer.source()];

			bool did_write;
			if constexpr (flags.transfer_byte()) {
				did_write = bus.template write<uint8_t>(address, uint8_t(source), registers_.mode(), trans);
			} else {
				// "The data presented to the data bus are not affected if the address is not word aligned".
				did_write = bus.template write<uint32_t>(address, source, registers_.mode(), trans);
			}

			if(!did_write) {
				registers_.exception<Registers::Exception::DataAbort>();
				return;
			}
		} else {
			bool did_read;
			uint32_t value;

			if constexpr (flags.transfer_byte()) {
				uint8_t target;
				did_read = bus.template read<uint8_t>(address, target, registers_.mode(), trans);
				value = target;
			} else {
				did_read = bus.template read<uint32_t>(address, value, registers_.mode(), trans);

				// "An address offset from a word boundary will cause the data to be rotated into the
				// register so that the addressed byte occuplies bits 0 to 7."
				switch(address & 3) {
					case 0:	break;
					case 1:	value = (value >> 8) | (value << 24);	break;
					case 2:	value = (value >> 16) | (value << 16);	break;
					case 3:	value = (value >> 24) | (value << 8);	break;
				}
			}

			if(!did_read) {
				registers_.exception<Registers::Exception::DataAbort>();
				return;
			}

			if(transfer.destination() == 15) {
				registers_.set_pc(value);
			} else {
				registers_.active[transfer.destination()] = value;
			}
		}

		// If either postindexing or else with writeback, update base.
		if constexpr (!flags.pre_index() || flags.write_back_address()) {
			if(transfer.base() == 15) {
				registers_.set_pc(offsetted_address);
			} else {
				registers_.active[transfer.base()] = offsetted_address;
			}
		}
	}
	template <Flags f> void perform(BlockDataTransfer transfer) {
		constexpr BlockDataTransferFlags flags(f);

		// Grab a copy of the list of registers to transfer.
		const uint16_t list = transfer.register_list();

		// Read the base address and take a copy in case a data abort means that
		// it has to be restored later, and to write that value rather than
		// the final address if the base register is first in the write-out list.
		uint32_t address = transfer.base() == 15 ?
			registers_.pc_status(4) :
			registers_.active[transfer.base()];
		const uint32_t initial_address = address;

		// Figure out what the final address will be, since that's what'll be
		// in the output if the base register is second or beyond in the
		// write-out list.
		//
		// Writes are always ordered from lowest address to highest; adjust the
		// start address if this write is supposed to fill memory downward from
		// the base.

		// TODO: use std::popcount when adopting C++20.
		uint32_t total = ((list & 0xa) >> 1) + (list & 0x5);
		total = ((list & 0xc) >> 2) + (list & 0x3);

		uint32_t final_address;
		if constexpr (!flags.add_offset()) {
			final_address = address + total * 4;
			address = final_address;
		} else {
			final_address = address + total * 4;
		}

		// For loads, keep a record of the value replaced by the last load and
		// where it came from. A data abort cancels both the current load and
		// the one before it, so this is used by this implementation to undo
		// the previous load in that case.
		struct {
			uint32_t *target = nullptr;
			uint32_t value;
		} last_replacement;

		// Check whether access is forced ot the user bank; if so then switch
		// to it now. Also keep track of the original mode to switch back at
		// the end.
		const Mode original_mode = registers_.mode();
		const bool adopt_user_mode =
			(
				flags.operation() == BlockDataTransferFlags::Operation::STM &&
				flags.load_psr()
			) ||
			(
				flags.operation() == BlockDataTransferFlags::Operation::LDM &&
				!(list & (1 << 15))
			);
		if(adopt_user_mode) {
			registers_.set_mode(Mode::User);
		}

		bool address_error = false;

		// Keep track of whether all accesses succeeded in order potentially to
		// throw a data abort later.
		bool accesses_succeeded = true;
		const auto access = [&](uint32_t &value) {
			// Update address in advance for:
			//	* pre-indexed upward stores; and
			//	* post-indxed downward stores.
			if constexpr (flags.pre_index() == flags.add_offset()) {
				address += 4;
			}

			if constexpr (flags.operation() == BlockDataTransferFlags::Operation::STM) {
				if(!address_error) {
					// "If the abort occurs during a store multiple instruction, ARM takes little action until
					// the instruction completes, whereupon it enters the data abort trap. The memory manager is
					// responsible for preventing erroneous writes to the memory."
					accesses_succeeded &= bus.template write<uint32_t>(address, value, registers_.mode(), false);
				}
			} else {
				// When ARM detects a data abort during a load multiple instruction, it modifies the operation of
				// the instruction to ensure that recovery is possible.
				//
				//	*	Overwriting of registers stops when the abort happens. The aborting load will not
				//		take place, nor will the preceding one ...
				//	*	The base register is restored, to its modified value if write-back was requested.
				if(accesses_succeeded) {
					const uint32_t replaced = value;
					accesses_succeeded &= bus.template read<uint32_t>(address, value, registers_.mode(), false);

					// Update the last-modified slot if the access succeeded; otherwise
					// undo the last modification if there was one, and undo the base
					// address change.
					if(accesses_succeeded) {
						last_replacement.value = replaced;
						last_replacement.target = &value;
					} else {
						if(last_replacement.target) {
							*last_replacement.target = last_replacement.value;
						}

						// Also restore the base register.
						if(transfer.base() != 15) {
							if constexpr (flags.write_back_address()) {
								registers_.active[transfer.base()] = final_address;
							} else {
								registers_.active[transfer.base()] = initial_address;
							}
						}
					}
				} else {
					// Implicitly: do the access anyway, but don't store the value. I think.
					uint32_t throwaway;
					bus.template read<uint32_t>(address, throwaway, registers_.mode(), false);
				}
			}

			// Update address after the fact for:
			//	* post-indexed upward stores; and
			//	* pre-indxed downward stores.
			if constexpr (flags.pre_index() != flags.add_offset()) {
				address += 4;
			}
		};

		// Check for an address exception.
		address_error = address >= (1 << 26);

		// Write out registers 1 to 14.
		for(int c = 0; c < 15; c++) {
			if(list & (1 << c)) {
				access(registers_.active[c]);

				// Modify base register after each write if writeback is enabled.
				// This'll ensure the unmodified value goes out if it was the
				// first-selected register only.
				if constexpr (flags.write_back_address()) {
					if(transfer.base() != 15) {
						registers_.active[transfer.base()] = final_address;
					}
				}
			}
		}

		// Definitively write back, even if the earlier register list
		// was empty.
		if constexpr (flags.write_back_address()) {
			if(transfer.base() != 15) {
				registers_.active[transfer.base()] = final_address;
			}
		}

		// Read or write the program counter as a special case if it was in the list.
		if(list & (1 << 15)) {
			uint32_t value;
			if constexpr (flags.operation() == BlockDataTransferFlags::Operation::STM) {
				value = registers_.pc_status(8);
				access(value);
			} else {
				access(value);
				registers_.set_pc(value);
				if constexpr (flags.load_psr()) {
					registers_.set_status(value);
				}
			}
		}

		// If user mode was unnaturally forced, switch back to the actual
		// current operating mode.
		if(adopt_user_mode) {
			registers_.set_mode(original_mode);
		}

		// Finally throw an exception if necessary.
		if(address_error) {
			registers_.exception<Registers::Exception::Address>();
		} else if(!accesses_succeeded) {
			registers_.exception<Registers::Exception::DataAbort>();
		}
	}

	void software_interrupt() {
		registers_.exception<Registers::Exception::SoftwareInterrupt>();
	}
	void unknown() {
		registers_.exception<Registers::Exception::UndefinedInstruction>();
	}

	// Act as if no coprocessors present.
	template <Flags> void perform(CoprocessorRegisterTransfer) {
		registers_.exception<Registers::Exception::UndefinedInstruction>();
	}
	template <Flags> void perform(CoprocessorDataOperation) {
		registers_.exception<Registers::Exception::UndefinedInstruction>();
	}
	template <Flags> void perform(CoprocessorDataTransfer) {
		registers_.exception<Registers::Exception::UndefinedInstruction>();
	}

	MemoryT bus;

	/// Sets the expected address of the instruction after whichever  is about to be executed.
	/// So it's PC+4 compared to most other systems.
	void set_pc(uint32_t pc) {
		registers_.set_pc(pc);
	}

	/// @returns The address of the instruction that should be fetched next. So as execution of each instruction
	/// begins, this will be +4 from the instruction being executed; at the end of the instruction it'll either still be +4
	/// or else be some other address if a branch or exception has occurred.
	uint32_t pc() const {
		return registers_.pc(0);
	}

	/// @returns The current processor mode.
	Mode mode() const {
		return registers_.mode();
	}

private:
	Registers registers_;
};

/// Provides an analogue of the @c OperationMapper -affiliated @c dispatch that also updates the
/// executor's program counter appropriately.
template <Model model, typename MemoryT>
void execute(uint32_t instruction, Executor<model, MemoryT> &executor) {
	executor.set_pc(executor.pc() + 4);
	dispatch<model>(instruction, executor);
}

}
