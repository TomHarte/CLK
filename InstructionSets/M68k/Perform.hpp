//
//  Perform.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 28/04/2022.
//  Copyright © 2022 Thomas Harte. All rights reserved.
//

#ifndef InstructionSets_M68k_Perform_h
#define InstructionSets_M68k_Perform_h

#include "Model.hpp"
#include "Instruction.hpp"
#include "Status.hpp"
#include "../../Numeric/RegisterSizes.hpp"

namespace InstructionSet {
namespace M68k {

struct NullFlowController {
	//
	// Various operation-specific did-perform notfications; these all relate to operations
	// with variable timing on a 68000, providing the fields that contribute to that timing.
	//

	/// Indicates that a @c MULU was performed, providing the @c source operand.
	template <typename IntT> void did_mulu(IntT) {}

	/// Indicates that a @c MULS was performed, providing the @c source operand.
	template <typename IntT> void did_muls(IntT) {}

	/// Indicates that a @c CHK was performed, along with whether the result @c was_under zero or @c was_over the source operand.
	void did_chk([[maybe_unused]] bool was_under, [[maybe_unused]] bool was_over) {}

	/// Indicates an in-register shift or roll occurred, providing the number of bits shifted by.
	void did_shift([[maybe_unused]] int bit_count) {}

	/// Indicates that a @c DIVU was performed, providing the @c dividend and @c divisor.
	/// If @c did_overflow is @c true then the divide ended in overflow.
	template <bool did_overflow> void did_divu([[maybe_unused]] uint32_t dividend, [[maybe_unused]] uint32_t divisor) {}

	/// Indicates that a @c DIVS was performed, providing the @c dividend and @c divisor.
	/// If @c did_overflow is @c true then the divide ended in overflow.
	template <bool did_overflow> void did_divs([[maybe_unused]] int32_t dividend, [[maybe_unused]] int32_t divisor) {}

	/// Indicates that a bit-manipulation operation (i.e. BTST, BCHG or BSET) was performed, affecting the bit at posiition @c bit_position.
	void did_bit_op([[maybe_unused]] int bit_position) {}

	/// Provides a notification that the upper byte of the status register has been affected by the current instruction;
	/// this gives an opportunity to track the supervisor flag.
	void did_update_status() {}

	//
	// Operations that don't fit the reductive load-modify-store pattern; these are requests from perform
	// that the flow controller do something (and, correspondingly, do not have empty implementations).
	//
	// All offsets are the native values as encoded in the corresponding operations.
	//

	/// If @c matched_condition is @c true, apply the @c offset to the PC.
	template <typename IntT> void complete_bcc(bool matched_condition, IntT offset);

	/// If both @c matched_condition and @c overflowed are @c false, apply @c offset to the PC.
	void complete_dbcc(bool matched_condition, bool overflowed, int16_t offset);

	/// Push the program counter of the next instruction to the stack, and add @c offset to the PC.
	void bsr(uint32_t offset);

	/// Push the program counter of the next instruction to the stack, and load @c offset to the PC.
	void jsr(uint32_t address);

	/// Set the program counter to @c address.
	void jmp(uint32_t address);

	/// Pop a word from the stack and use that to set the status condition codes. Then pop a new value for the PC.
	void rtr();

	/// Pop a word from the stack and use that to set the entire status register. Then pop a new value for the PC.
	void rte();

	/// Pop a new value for the PC from the stack.
	void rts();

	/// Put the processor into the stopped state, waiting for interrupts.
	void stop();

	/// Perform LINK using the address register identified by @c instruction and the specified @c offset.
	void link(Preinstruction instruction, uint32_t offset);

	/// Perform unlink, with @c address being the target address register.
	void unlink(uint32_t &address);

	/// Push @c address to the stack.
	void pea(uint32_t address);

	/// Perform an atomic TAS cycle; if @c instruction indicates that this is a TAS Dn then
	/// perform the TAS directly upon that register; otherwise perform it on the memory at
	/// @c address. If this is a TAS Dn then @c address will contain the initial value of
	/// the register.
	void tas(Preinstruction instruction, uint32_t address);

	/// Use @c instruction to determine the direction of this MOVEP and perform it;
	/// @c source is the first operand provided to the MOVEP — either an address or register
	/// contents — and @c dest is the second.
	///
	/// @c IntT may be either uint16_t or uint32_t.
	template <typename IntT> void movep(Preinstruction instruction, uint32_t source, uint32_t dest);

	/// Perform a MOVEM to memory, from registers. @c instruction will indicate the mask as the first operand,
	/// and the target address and addressing mode as the second; the mask and address are also supplied
	/// as @c mask and @c address. If the addressing mode is -(An) then the address register will have
	/// been decremented already.
	///
	/// The receiver is responsible for updating the address register if applicable.
	///
	/// @c IntT may be either uint16_t or uint32_t.
	template <typename IntT> void movem_toM(Preinstruction instruction, uint32_t mask, uint32_t address);

	/// Perform a MOVEM to registers, from memory. @c instruction will indicate the mask as the first operand,
	/// and the target address and addressing mode as the second; the mask and address are also supplied
	/// as @c mask and @c address. If the addressing mode is (An)+ then the address register will have been
	/// incremented, but @c address will be its value before that occurred.
	///
	/// The receiver is responsible for updating the address register if applicable.
	///
	/// @c IntT may be either uint16_t or uint32_t.
	template <typename IntT> void movem_toR(Preinstruction instruction, uint32_t mask, uint32_t address);

	/// Raises a short-form exception using @c vector. If @c use_current_instruction_pc is @c true,
	/// the program counter for the current instruction is included in the resulting stack frame. Otherwise the program
	/// counter for the next instruction is used.
	template <bool use_current_instruction_pc = true>
	void raise_exception([[maybe_unused]] int vector);
};

/// Performs @c instruction using @c source and @c dest (one or both of which may be ignored as per
/// the semantics of the operation).
///
/// Any change in processor status will be applied to @c status. If this operation does not fit the reductive model
/// of being a read and possibly a modify and possibly a write of up to two operands then the @c flow_controller
/// will be asked to fill in the gaps.
///
/// If the template parameter @c operation is not @c Operation::Undefined then that operation will be performed, ignoring
/// whatever is specifed in @c instruction. This allows selection either at compile time or at run time; per Godbolt all modern
/// compilers seem to be smart enough fully to optimise the compile-time case.
template <
	Model model,
	typename FlowController,
	Operation operation = Operation::Undefined
> void perform(Preinstruction instruction, CPU::RegisterPair32 &source, CPU::RegisterPair32 &dest, Status &status, FlowController &flow_controller);

}
}

#include "Implementation/PerformImplementation.hpp"

#endif /* InstructionSets_M68k_Perform_h */
