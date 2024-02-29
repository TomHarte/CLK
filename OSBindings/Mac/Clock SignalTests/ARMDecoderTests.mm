//
//  ARMDecoderTests.m
//  Clock Signal
//
//  Created by Thomas Harte on 16/02/2024.
//  Copyright 2024 Thomas Harte. All rights reserved.
//

#import <XCTest/XCTest.h>

#include "../../../InstructionSets/ARM/BarrelShifter.hpp"
#include "../../../InstructionSets/ARM/OperationMapper.hpp"
#include "../../../InstructionSets/ARM/Registers.hpp"
#include "../../../Numeric/Carry.hpp"

using namespace InstructionSet::ARM;

namespace {

struct Scheduler {
	bool should_schedule(Condition condition) {
		return registers_.test(condition);
	}

	template <bool allow_register, bool set_carry, typename FieldsT>
	uint32_t decode_shift(FieldsT fields, uint32_t &rotate_carry, uint32_t pc_offset) {
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
						registers_.pc(8) :
						registers_.active[fields.shift_register()];
			} else {
				shift_amount = fields.shift_amount();
			}
		} else {
			shift_amount = fields.shift_amount();
		}

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
				registers_.pc(shift_by_register ? 12 : 8) :
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
			operand2 = decode_shift<true, shift_sets_carry>(fields, rotate_carry, shift_by_register ? 12 : 8);
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

		const uint32_t multiplicand = fields.multiplicand() == 15 ? registers_.pc(8) : registers_.active[fields.multiplicand()];
		const uint32_t multiplier = fields.multiplier() == 15 ? registers_.pc_status(8) : registers_.active[fields.multiplier()];
		const uint32_t accumulator =
			flags.operation() == MultiplyFlags::Operation::MUL ? 0 :
				(fields.multiplicand() == 15 ? registers_.pc_status(12) : registers_.active[fields.accumulator()]);

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
			registers_.active[14] = registers_.pc(4);
		}
		registers_.set_pc(registers_.pc(8) + branch.offset());
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
			offset = decode_shift<false, false>(transfer, carry, 8);
		}

		// Obtain base address.
		uint32_t address =
			transfer.base() == 15 ?
				registers_.pc(8) :
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

		// TODO: attempt access, possibly abort.
		// Cf. transfer_byte()

		// If either postindexing or else with writeback, update base.
		if constexpr (!flags.pre_index() || flags.write_back_address()) {
			// TODO: check for R15.
			if(transfer.base() == 15) {
				registers_.set_pc(offsetted_address);
			} else {
				registers_.active[transfer.base()] = offsetted_address;
			}
		}
	}
	template <Flags> void perform(BlockDataTransfer) {}

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

private:
	Registers registers_;
};

}

@interface ARMDecoderTests : XCTestCase
@end

@implementation ARMDecoderTests

- (void)testXYX {
	Scheduler scheduler;

	for(int c = 0; c < 65536; c++) {
		InstructionSet::ARM::dispatch(c << 16, scheduler);
	}
	InstructionSet::ARM::dispatch(0xEAE06900, scheduler);
}

@end
