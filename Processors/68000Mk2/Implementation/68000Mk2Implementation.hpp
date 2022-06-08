//
//  68000Mk2Implementation.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 16/05/2022.
//  Copyright © 2022 Thomas Harte. All rights reserved.
//

#ifndef _8000Mk2Implementation_h
#define _8000Mk2Implementation_h

#include <cassert>
#include <cstdio>

#include "../../../InstructionSets/M68k/ExceptionVectors.hpp"

namespace CPU {
namespace MC68000Mk2 {

/// States for the state machine which are named by
/// me for their purpose rather than automatically by file position.
/// These are negative to avoid ambiguity with the other group.
enum ExecutionState: int {
	Reset			= std::numeric_limits<int>::min(),
	Decode,
	WaitForDTACK,

	/// Perform the proper sequence to fetch a byte or word operand.
	FetchOperand_bw,
	/// Perform the proper sequence to fetch a long-word operand.
	FetchOperand_l,

	StoreOperand,
	StoreOperand_bw,
	StoreOperand_l,

	StandardException,
	BusOrAddressErrorException,
	DoInterrupt,

	// Specific addressing mode fetches.
	//
	// Additional context here is that I'm very much on the fence but
	// for now am telling myself:
	//
	//	(1)	the overwhelming majority of instructions that need an
	//		effective address calculation use it for an operand read
	//		immediately afterwards, so keeping those things bound
	//		avoids a large number of conditional branches; and
	//	(2)	making a decision between byte/word and long-word once at
	//		the outset also saves a conditional for any two-operand
	//		instructions (which is also the majority); but
	//	(3)	some instructions do just need the address calculation —
	//		LEA and PEA are obvious examples, but are not the
	//		exhaustive list — so a third route just to do the
	//		calculation is necessary.
	//
	// My internal dialogue then argues that each of these is actually
	// a small amount of code, so the need manually to duplicate (per
	// the control-flow constraints of using a switch as a coroutine)
	// isn't too ugly. Possibly even less ugly than pulling things out
	// with a macro, especially for debugging.
	//
	// Further consideration may be necessary. Especially once this is
	// up on its feet and profiling becomes an option.

	FetchAddressRegisterIndirect_bw,
	FetchAddressRegisterIndirectWithPostincrement_bw,
	FetchAddressRegisterIndirectWithPredecrement_bw,
	FetchAddressRegisterIndirectWithDisplacement_bw,
	FetchAddressRegisterIndirectWithIndex8bitDisplacement_bw,
	FetchProgramCounterIndirectWithDisplacement_bw,
	FetchProgramCounterIndirectWithIndex8bitDisplacement_bw,
	FetchAbsoluteShort_bw,
	FetchAbsoluteLong_bw,
	FetchImmediateData_bw,

	FetchAddressRegisterIndirect_l,
	FetchAddressRegisterIndirectWithPostincrement_l,
	FetchAddressRegisterIndirectWithPredecrement_l,
	FetchAddressRegisterIndirectWithDisplacement_l,
	FetchAddressRegisterIndirectWithIndex8bitDisplacement_l,
	FetchProgramCounterIndirectWithDisplacement_l,
	FetchProgramCounterIndirectWithIndex8bitDisplacement_l,
	FetchAbsoluteShort_l,
	FetchAbsoluteLong_l,
	FetchImmediateData_l,

	CalcEffectiveAddress,											// -
	CalcAddressRegisterIndirect,									// -
	CalcAddressRegisterIndirectWithPostincrement,					// -
	CalcAddressRegisterIndirectWithPredecrement,					// -
	CalcAddressRegisterIndirectWithDisplacement,					// np
	CalcAddressRegisterIndirectWithIndex8bitDisplacement,			// np n
	CalcProgramCounterIndirectWithDisplacement,						// np
	CalcProgramCounterIndirectWithIndex8bitDisplacement,			// np n
	CalcAbsoluteShort,												// np
	CalcAbsoluteLong,												// np np

	CalcEffectiveAddressIdleFor8bitDisplacement,					// As per CalcEffectiveAddress unless one of the
																	// 8-bit displacement modes is in use, in which case
																	// an extra idle bus state is prefixed.

	// Various forms of perform; each of these will
	// perform the current instruction, then do the
	// indicated bus cycle.

	Perform_np,
	Perform_np_n,
	Perform_np_nn,

	MOVE,
	MOVE_predec,
	MOVE_predec_l,
	MOVE_prefetch_decode,
	MOVE_complete,
	MOVE_complete_l,

	TwoOp_Predec_bw,
	TwoOp_Predec_l,

	CHK,
	CHK_no_trap,
	CHK_was_over,
	CHK_was_under,

	Scc_Dn,
	Scc_Dn_did_not_set,
	Scc_Dn_did_set,

	DBcc,
	DBcc_branch_taken,
	DBcc_condition_true,
	DBcc_counter_overflow,

	Bccb,
	Bccw,
	Bcc_branch_taken,
	Bccb_branch_not_taken,
	Bccw_branch_not_taken,

	BSRb,
	BSRw,

	JSRJMPAddressRegisterIndirect,
	JSRJMPAddressRegisterIndirectWithDisplacement,
	JSRJMPAddressRegisterIndirectWithIndex8bitDisplacement,
	JSRJMPProgramCounterIndirectWithDisplacement,
	JSRJMPProgramCounterIndirectWithIndex8bitDisplacement,
	JSRJMPAbsoluteShort,
	JSRJMPAbsoluteLong,

	JSR, JMP,

	BCHG_BSET_Dn,
	BCLR_Dn,

	MOVEPtoM_w,
	MOVEPtoM_l,
	MOVEPtoR_w,
	MOVEPtoR_l,

	LogicalToSR,

	MOVEMtoR, MOVEMtoR_l_read, MOVEMtoR_w_read, MOVEMtoR_finish,
	MOVEMtoM,
		MOVEMtoM_l_write, MOVEMtoM_w_write,
		MOVEMtoM_l_write_predec, MOVEMtoM_w_write_predec,
	MOVEMtoM_finish,

	DIVU_DIVS,
	Perform_idle_dyamic_Dn,
	LEA,
	PEA,
	TAS,
	MOVEtoCCRSR,
	RTR,
	RTE,
	RTS,
	LINKw,
	UNLINK,
	RESET,
	NOP,
	STOP,
	TRAP,
	TRAPV,
};

// MARK: - The state machine.

template <class BusHandler, bool dtack_is_implicit, bool permit_overrun, bool signal_will_perform>
void Processor<BusHandler, dtack_is_implicit, permit_overrun, signal_will_perform>::run_for(HalfCycles duration) {
	// Accumulate the newly paid-in cycles. If this instance remains in deficit, exit.
	e_clock_phase_ += duration;
	time_remaining_ += duration;
	if(time_remaining_ < HalfCycles(0)) return;

	// Check whether all remaining time has been expended; if so then exit, having set this line up as
	// the next resumption point.
#define ConsiderExit()	if(time_remaining_ < HalfCycles(0)) { state_ = __COUNTER__+1; return; } [[fallthrough]]; case __COUNTER__:

	// Subtracts `n` half-cycles from `time_remaining_`; if permit_overrun is false, also ConsiderExit()
#define Spend(n)		time_remaining_ -= (n); if constexpr (!permit_overrun) ConsiderExit()

	// Performs ConsiderExit() only if permit_overrun is true.
#define CheckOverrun()	if constexpr (permit_overrun) ConsiderExit()

	// Moves directly to state x, which must be a compile-time constant.
#define MoveToStateSpecific(x)	goto x;

	// Moves to state x by dynamic dispatch; x can be a regular variable.
#define MoveToStateDynamic(x)	{ state_ = x; continue; }

	// Sets the start position for state x.
#define BeginState(x)	case ExecutionState::x: [[maybe_unused]] x

	//
	// So basic structure is, in general:
	//
	//	BeginState(Action):
	//		do_something();
	//		Spend(20);
	//		do_something_else();
	//		Spend(10);
	//		do_a_third_thing();
	//		Spend(30);
	//	MoveToState(next_action);
	//
	// Additional notes:
	//
	//	Action and all equivalents should be negative values, since the
	//	switch-for-computed-goto-for-a-coroutine structure uses __COUNTER__* for
	//	its invented entry- and exit-points, meaning that negative numbers are
	//	the easiest group that is safely definitely never going to collide.
	//
	//	(* an extension supported by at least GCC, Clang and MSVC)


	// Spare containers:
	HalfCycles delay;		// To receive any additional time added on by calls to perform_bus_operation.

	// Helper macros for common bus transactions:

	// Raises the exception with integer vector x. x is the vector identifier,
	// not its address.
#define RaiseException(x)					\
	exception_vector_ = x;					\
	MoveToStateSpecific(StandardException);

	// Raises a bus/address error with integer vector x for access v.
	// x is the vector identifier, not its address.
#define RaiseBusOrAddressError(x, v)				\
	exception_vector_ = InstructionSet::M68k::x;	\
	bus_error_ = v;									\
	MoveToStateSpecific(BusOrAddressErrorException);

	// Performs the bus operation and then applies a `Spend` of its length
	// plus any additional length returned by the bus handler.
#define PerformBusOperation(x)										\
	delay = bus_handler_.perform_bus_operation(x, is_supervisor_);	\
	Spend(x.length + delay)

	// Performs no bus activity for the specified number of microcycles.
#define IdleBus(n)						\
	idle.length = HalfCycles((n) << 2);	\
	PerformBusOperation(idle)

	// Spin until DTACK, VPA or BERR is asserted (unless DTACK is implicit),
	// holding the bus cycle provided.
#define WaitForDTACK(x)													\
	if constexpr (!dtack_is_implicit && !dtack_ && !vpa_ && !berr_) {	\
		awaiting_dtack = x;												\
		awaiting_dtack.length = HalfCycles(2);							\
		post_dtack_state_ = __COUNTER__+1;								\
		state_ = ExecutionState::WaitForDTACK;							\
		break;															\
	}																	\
	[[fallthrough]]; case __COUNTER__:

	// Performs the bus operation provided, which will be one with a
	// SelectWord or SelectByte operation, stretching it to match the E
	// bus if VPA is currently asserted or seguing elsewhere if a bus
	// error is signalled or an adress error observed.
	//
	// E clock behaviour implemented, which I think is correct:
	//
	//	(1) wait until end of current 10-cycle window;
	//	(2) run for the next 10-cycle window.
#define CompleteAccess(x)												\
	if(berr_) {															\
		RaiseBusOrAddressError(AccessFault, x);							\
	}																	\
	if(vpa_) {															\
		x.length = HalfCycles(20) + (HalfCycles(20) + (e_clock_phase_ - time_remaining_) % HalfCycles(20)) % HalfCycles(20);	\
	} else {															\
		x.length = HalfCycles(4);										\
	}																	\
	PerformBusOperation(x)

	// Performs the memory access implied by the announce, perform pair,
	// honouring DTACK, BERR and VPA as necessary.
#define AccessPair(val, announce, perform)					\
	perform.value = &val;									\
	if constexpr (!dtack_is_implicit) {						\
		announce.length = HalfCycles(4);					\
	}														\
	if(*perform.address & (perform.operation >> 1) & 1) {	\
		RaiseBusOrAddressError(AddressError, perform);		\
	}														\
	PerformBusOperation(announce);							\
	WaitForDTACK(announce);									\
	CompleteAccess(perform);

	// Sets up the next data access size and read flags.
#define SetupDataAccess(read_flag, select_flag)												\
	access_announce.operation = Microcycle::NewAddress | Microcycle::IsData | (read_flag);	\
	access.operation = access_announce.operation | (select_flag);

	// Sets the address source for the next data access.
#define SetDataAddress(addr)							\
	access.address = access_announce.address = &addr;

	// Performs the access established by SetupDataAccess into val.
#define Access(val)										\
	AccessPair(val, access_announce, access)

	// Reads the program (i.e. non-data) word from addr into val.
#define ReadProgramWord(val)								\
	AccessPair(val, read_program_announce, read_program);	\
	program_counter_.l += 2;

	// Reads one futher word from the program counter and inserts it into
	// the prefetch queue.
#define Prefetch()										\
	prefetch_.high = prefetch_.low;						\
	ReadProgramWord(prefetch_.low)						\
	captured_interrupt_level_ = bus_interrupt_level_;

	// Copies the current program counter, adjusted to allow for the prefetch queue,
	// into the instruction_address_ latch, which is the source of the value written
	// during exceptions.
#define ReloadInstructionAddress()	\
	instruction_address_.l = program_counter_.l - 4

	using Mode = InstructionSet::M68k::AddressingMode;

	// Otherwise continue for all time, until back in debt.
	// Formatting is slightly obtuse here to make this look more like a coroutine.
	while(true) { switch(state_) {

		// Spin in place, one cycle at a time, until one of DTACK,
		// BERR or VPA is asserted.
		BeginState(WaitForDTACK):
			PerformBusOperation(awaiting_dtack);

			if(dtack_ || berr_ || vpa_) {
				MoveToStateDynamic(post_dtack_state_);
			}
		MoveToStateSpecific(WaitForDTACK);

		// Spin in place until an interrupt arrives.
		BeginState(STOP):
			IdleBus(1);
			captured_interrupt_level_ = bus_interrupt_level_;
			if(status_.would_accept_interrupt(captured_interrupt_level_)) {
				MoveToStateSpecific(DoInterrupt);
			}
		MoveToStateSpecific(STOP);

		// Perform the RESET exception, which seeds the stack pointer and program
		// counter, populates the prefetch queue, and then moves to instruction dispatch.
		BeginState(Reset):
			IdleBus(7);			// (n-)*5   nn

			// Establish general reset state.
			status_.begin_exception(7);
			should_trace_ = 0;
			did_update_status();

			SetupDataAccess(Microcycle::Read, Microcycle::SelectWord);
			SetDataAddress(temporary_address_.l);

			temporary_address_.l = 0;
			Access(registers_[15].high);	// nF

			temporary_address_.l += 2;
			Access(registers_[15].low);		// nf

			temporary_address_.l += 2;
			Access(program_counter_.high);	// nV

			temporary_address_.l += 2;
			Access(program_counter_.low);	// nv

			Prefetch();			// np
			IdleBus(1);			// n
			Prefetch();			// np
		MoveToStateSpecific(Decode);

		// Perform a 'standard' exception, i.e. a Group 1 or 2.
		BeginState(StandardException):
			// Switch to supervisor mode, disable interrupts.
			captured_status_.w = status_.begin_exception();
			should_trace_ = 0;
			did_update_status();

			SetupDataAccess(0, Microcycle::SelectWord);
			SetDataAddress(registers_[15].l);

			// Push status and current program counter.
			// Write order is wacky here, but I think it's correct.
			registers_[15].l -= 6;
			Access(captured_status_);			// ns

			registers_[15].l += 4;
			Access(instruction_address_.low);	// ns

			registers_[15].l -= 2;
			Access(instruction_address_.high);	// nS

			registers_[15].l -= 2;

			// Grab new program counter.
			SetupDataAccess(Microcycle::Read, Microcycle::SelectWord);
			SetDataAddress(temporary_address_.l);

			temporary_address_.l = uint32_t(exception_vector_ << 2);
			Access(program_counter_.high);	// nV

			temporary_address_.l += 2;
			Access(program_counter_.low);	// nv

			// Populate the prefetch queue.
			Prefetch();			// np
			IdleBus(1);			// n
			Prefetch();			// np
		MoveToStateSpecific(Decode);

		BeginState(BusOrAddressErrorException):
			// "The microcode pushes the stack frame in a non consecutive order"
			// per Ijor's document, but little further information is given.
			//
			// So the below is a cross-your-fingers guess based on the constraints
			// that the information writen, from lowest address to highest is:
			//
			// 	R/W, I/N, function code word;		[at -14]
			//	access address;						[-12]
			//	instruction register;				[-8]
			//	status register;					[-6]
			//	program counter.					[-4]
			//
			// With the instruction register definitely being written before the
			// function code word.
			//
			// And the documented bus pattern is:
			//
			// nn ns ns nS ns ns ns nS nV nv np  n np
			//
			// So, based on the hoopy ordering of a standard exception, maybe:
			//
			//	1) program counter low;
			//	2) captured state;
			//	3) program counter high;
			//	4) instruction register;
			//	5) function code;
			//	6) access address?

			IdleBus(2);

			// Switch to supervisor mode, disable interrupts.
			captured_status_.w = status_.begin_exception(7);
			should_trace_ = 0;
			did_update_status();

			SetupDataAccess(0, Microcycle::SelectWord);
			SetDataAddress(registers_[15].l);

			// Guess: the written program counter is adjusted to discount the prefetch queue.
			// COMPLETE GUESS.
			temporary_address_.l = program_counter_.l - 4;
			registers_[15].l -= 2;
			Access(temporary_address_.low);		// ns	[pc.l]

			registers_[15].l -= 4;
			Access(captured_status_);			// ns	[sr]

			registers_[15].l += 2;
			Access(temporary_address_.high);	// nS	[pc.h]

			registers_[15].l -= 4;
			temporary_value_.w = opcode_;
			Access(temporary_value_.low);		// ns	[instruction register]

			// Construct the function code; which is:
			//
			// b4: 1 = was a read; 0 = was a write;
			// b3: 0 = was reading an instruction; 1 = wasn't;
			// b2–b0: the regular 68000 function code;
			// [all other bits]: left over from the instruction register write, above.
			//
			// I'm unable to come up with a reason why the function code isn't duplicative
			// of b3, but given the repetition of supervisor state which is also in the
			// captured status register I guess maybe it is just duplicative.
			temporary_value_.w =
				(temporary_value_.w & ~31) |
				((bus_error_.operation & Microcycle::Read) ? 0x10 : 0x00) |
				((bus_error_.operation & Microcycle::IsProgram) ? 0x08 : 0x00) |
				((bus_error_.operation & Microcycle::IsProgram) ? 0x02 : 0x01) |
				((captured_status_.w & InstructionSet::M68k::ConditionCode::Supervisor) ? 0x04 : 0x00);

			registers_[15].l -= 6;
			Access(temporary_value_.low);		// ns	[function code]

			temporary_address_.l = *bus_error_.address;
			registers_[15].l += 4;
			Access(temporary_address_.low);		// ns	[error address.l]

			registers_[15].l -= 2;
			Access(temporary_address_.high);	// nS	[error address.h]
			registers_[15].l -= 2;

			// Grab new program counter.
			SetupDataAccess(Microcycle::Read, Microcycle::SelectWord);
			SetDataAddress(temporary_address_.l);

			temporary_address_.l = uint32_t(exception_vector_ << 2);
			Access(program_counter_.high);	// nV

			temporary_address_.l += 2;
			Access(program_counter_.low);	// nv

			// Populate the prefetch queue.
			Prefetch();			// np
			IdleBus(1);			// n
			Prefetch();			// np
		MoveToStateSpecific(Decode);

		// Acknowledge an interrupt, thereby obtaining an exception vector,
		// and do the exception.
		BeginState(DoInterrupt):
			IdleBus(3);			// n nn

			// Capture status and switch to supervisor mode.
			captured_status_.w = status_.begin_exception(captured_interrupt_level_);
			should_trace_ = 0;
			did_update_status();

			// Prepare for stack activity.
			SetupDataAccess(0, Microcycle::SelectWord);
			SetDataAddress(registers_[15].l);

			// Push low part of program counter.
			registers_[15].l -= 2;
			Access(instruction_address_.low);	// ns

			// Do the interrupt cycle, to obtain a vector.
			temporary_address_.l = 0xffff'fff1 | uint32_t(captured_interrupt_level_ << 1);
			interrupt_cycles[0].address = interrupt_cycles[1].address = &temporary_address_.l;
			interrupt_cycles[0].value = interrupt_cycles[1].value = &temporary_value_.low;
			PerformBusOperation(interrupt_cycles[0]);
			CompleteAccess(interrupt_cycles[1]);		// ni

			// If VPA is set, autovector.
			if(vpa_) {
				temporary_value_.b = uint8_t(InstructionSet::M68k::Exception::InterruptAutovectorBase - 1 + captured_interrupt_level_);
			}

			// TODO: if bus error is set, treat interrupt as spurious.

			IdleBus(3);							// n- n

			// Do the rest of the stack work.
			SetDataAddress(registers_[15].l);

			registers_[15].l -= 4;
			Access(captured_status_);			// ns

			registers_[15].l += 2;
			Access(instruction_address_.high);	// nS
			registers_[15].l -= 2;

			// Grab new program counter.
			SetupDataAccess(Microcycle::Read, Microcycle::SelectWord);
			SetDataAddress(temporary_address_.l);

			temporary_address_.l = uint32_t(temporary_value_.b << 2);
			Access(program_counter_.high);		// nV

			temporary_address_.l += 2;
			Access(program_counter_.low);		// nv

			// Populate the prefetch queue.
			Prefetch();			// np
			IdleBus(1);			// n
			Prefetch();			// np
		MoveToStateSpecific(Decode);

		// Inspect the prefetch queue in order to decode the next instruction,
		// and segue into the fetching of operands.
		BeginState(Decode):
			CheckOverrun();

			// Capture the address of the next instruction.
			ReloadInstructionAddress();

			// Head off into an interrupt if one is found.
			if(status_.would_accept_interrupt(captured_interrupt_level_)) {
				MoveToStateSpecific(DoInterrupt);
			}

			// Potentially perform a trace.
			if(should_trace_) {
				RaiseException(InstructionSet::M68k::Exception::Trace);
			}

			// Capture the current trace flag.
			should_trace_ = status_.trace_flag;

			// Read and decode an opcode.
			opcode_ = prefetch_.high.w;
			instruction_ = decoder_.decode(opcode_);

			// Signal the bus handler if requested.
			if constexpr (signal_will_perform) {
				// Set the state to Decode, so that if the callee pulls any shenanigans in order
				// to force an exit here, the interpreter can resume without skipping a beat.
				//
				// signal_will_perform is overtly a debugging/testing feature.
				state_ = Decode;
				bus_handler_.will_perform(instruction_address_.l, opcode_);
			}

			// Ensure the first parameter is next fetched.
			next_operand_ = 0;

			/// If operation x requires supervisor privileges, checks whether the user is currently in supervisor mode;
			/// if not then raises a privilege violation exception.
#define CheckSupervisor(x)																													\
		if constexpr (InstructionSet::M68k::requires_supervisor<InstructionSet::M68k::Model::M68000>(InstructionSet::M68k::Operation::x)) {	\
			if(!status_.is_supervisor) {																									\
				RaiseException(InstructionSet::M68k::Exception::PrivilegeViolation);														\
			}																																\
		}

#define CASE(x)									\
	case InstructionSet::M68k::Operation::x:	\
		CheckSupervisor(x);						\
		operand_flags_ = InstructionSet::M68k::operand_flags<InstructionSet::M68k::Model::M68000, InstructionSet::M68k::Operation::x>();

#define StdCASE(x, y)	\
	CASE(x)	\
		y;	\
		\
		if constexpr (InstructionSet::M68k::operand_size<InstructionSet::M68k::Operation::x>() == InstructionSet::M68k::DataSize::LongWord) {	\
			SetupDataAccess(Microcycle::Read, Microcycle::SelectWord);	\
			MoveToStateSpecific(FetchOperand_l);	\
		} else {	\
			if constexpr (InstructionSet::M68k::operand_size<InstructionSet::M68k::Operation::x>() == InstructionSet::M68k::DataSize::Byte) {	\
				SetupDataAccess(Microcycle::Read, Microcycle::SelectByte);	\
			} else {	\
				SetupDataAccess(Microcycle::Read, Microcycle::SelectWord);	\
			}	\
			MoveToStateSpecific(FetchOperand_bw);	\
		}

#define Duplicate(x, y)	\
	case InstructionSet::M68k::Operation::x:	\
		static_assert(	\
			InstructionSet::M68k::operand_flags<InstructionSet::M68k::Model::M68000, InstructionSet::M68k::Operation::x>() ==		\
			InstructionSet::M68k::operand_flags<InstructionSet::M68k::Model::M68000, InstructionSet::M68k::Operation::y>() &&		\
			InstructionSet::M68k::operand_size<InstructionSet::M68k::Operation::x>() == 											\
			InstructionSet::M68k::operand_size<InstructionSet::M68k::Operation::y>() &&												\
			InstructionSet::M68k::requires_supervisor<InstructionSet::M68k::Model::M68000>(InstructionSet::M68k::Operation::x) == 	\
			InstructionSet::M68k::requires_supervisor<InstructionSet::M68k::Model::M68000>(InstructionSet::M68k::Operation::y)		\
		);																															\
		[[fallthrough]];

#define SpecialCASE(x)	case InstructionSet::M68k::Operation::x: CheckSupervisor(x); MoveToStateSpecific(x)

			switch(instruction_.operation) {
				case InstructionSet::M68k::Operation::Undefined:
					if(instruction_.operation == InstructionSet::M68k::Operation::Undefined) {
						switch(opcode_ & 0xf000) {
							default:
								exception_vector_ = InstructionSet::M68k::Exception::IllegalInstruction;
							break;
							case 0xa000:
								exception_vector_ = InstructionSet::M68k::Exception::Line1010;
							break;
							case 0xf000:
								exception_vector_ = InstructionSet::M68k::Exception::Line1111;
							break;
						}
						MoveToStateSpecific(StandardException);
					}

				StdCASE(NBCD, {
					if(instruction_.mode(0) == Mode::DataRegisterDirect) {
						perform_state_ = Perform_np_n;
					} else {
						perform_state_ = Perform_np;
					}
				})

				Duplicate(CLRb, NEGXb)	Duplicate(NEGb, NEGXb)	Duplicate(NOTb, NEGXb)
				StdCASE(NEGXb, 		perform_state_ = Perform_np);

				Duplicate(CLRw, NEGXw)	Duplicate(NEGw, NEGXw)	Duplicate(NOTw, NEGXw)
				StdCASE(NEGXw, 		perform_state_ = Perform_np);

				Duplicate(CLRl, NEGXl)	Duplicate(NEGl, NEGXl)	Duplicate(NOTl, NEGXl)
				StdCASE(NEGXl,
					if(instruction_.mode(0) == Mode::DataRegisterDirect) {
						perform_state_ = Perform_np_n;
					} else {
						perform_state_ = Perform_np;
					}
				);

				StdCASE(SWAP, 		perform_state_ = Perform_np);
				StdCASE(EXG, 		perform_state_ = Perform_np_n);

				StdCASE(EXTbtow, 	perform_state_ = Perform_np);
				StdCASE(EXTwtol, 	perform_state_ = Perform_np);

				StdCASE(MOVEb,		perform_state_ = MOVE);
				Duplicate(MOVEAw, MOVEw)
				StdCASE(MOVEw,		perform_state_ = MOVE);
				Duplicate(MOVEAl, MOVEl)
				StdCASE(MOVEl,		perform_state_ = MOVE);

				StdCASE(CMPb,		perform_state_ = Perform_np);
				StdCASE(CMPw,		perform_state_ = Perform_np);
				StdCASE(CMPl,
					perform_state_ = instruction_.mode(1) == Mode::DataRegisterDirect ? Perform_np_n : Perform_np
				);

				StdCASE(CMPAw,		perform_state_ = Perform_np_n);
				StdCASE(CMPAl,		perform_state_ = Perform_np_n);

				Duplicate(ANDb, ORb)	StdCASE(ORb,		perform_state_ = Perform_np);
				Duplicate(ANDw, ORw)	StdCASE(ORw,		perform_state_ = Perform_np);
				Duplicate(ANDl, ORl)	StdCASE(ORl, {
					if(instruction_.mode(1) == Mode::DataRegisterDirect) {
						switch(instruction_.mode(0)) {
							default:
								perform_state_ = Perform_np_n;
							break;
							case Mode::DataRegisterDirect:
							case Mode::ImmediateData:
								perform_state_ = Perform_np_nn;
							break;
						}
					} else {
						perform_state_ = Perform_np;
					}
				});

				StdCASE(EORb,		perform_state_ = Perform_np);
				StdCASE(EORw,		perform_state_ = Perform_np);
				StdCASE(EORl, {
					if(instruction_.mode(1) == Mode::DataRegisterDirect) {
						perform_state_ = Perform_np_nn;
					} else {
						perform_state_ = Perform_np;
					}
				})

				Duplicate(SBCD, ABCD)
				CASE(ABCD)
					if(instruction_.mode(0) == Mode::DataRegisterDirect) {
						perform_state_ = Perform_np_n;
						SetupDataAccess(Microcycle::Read, Microcycle::SelectByte);
						MoveToStateSpecific(FetchOperand_bw);
					} else {
						select_flag_ = Microcycle::SelectByte;
						MoveToStateSpecific(TwoOp_Predec_bw);
					}

				StdCASE(CHK,		perform_state_ = CHK);

				Duplicate(SUBb, ADDb)	StdCASE(ADDb,		perform_state_ = Perform_np)
				Duplicate(SUBw, ADDw)	StdCASE(ADDw,		perform_state_ = Perform_np)
				Duplicate(SUBl, ADDl)	StdCASE(ADDl, {
					if(instruction_.mode(0) == Mode::Quick) {
						perform_state_ = (
							instruction_.mode(1) == Mode::AddressRegisterDirect ||
							instruction_.mode(1) == Mode::DataRegisterDirect
						) ? Perform_np_nn : Perform_np;
					} else {
						if(instruction_.mode(1) != Mode::DataRegisterDirect) {
							perform_state_ = Perform_np;
						} else {
							switch(instruction_.mode(0)) {
								default:
									perform_state_ = Perform_np_n;
								break;
								case Mode::DataRegisterDirect:
								case Mode::AddressRegisterDirect:
								case Mode::ImmediateData:
									perform_state_ = Perform_np_nn;
								break;
							}
						}
					}
				})

				Duplicate(SUBAw, ADDAw)	StdCASE(ADDAw, perform_state_ = Perform_np_nn)
				Duplicate(SUBAl, ADDAl)	StdCASE(ADDAl, {
					switch(instruction_.mode(0)) {
						default:
							perform_state_ = Perform_np_n;
						break;
						case Mode::DataRegisterDirect:
						case Mode::AddressRegisterDirect:
						case Mode::ImmediateData:
							perform_state_ = Perform_np_nn;
						break;
					}
				})

				Duplicate(SUBXb, ADDXb)	StdCASE(ADDXb, {
					if(instruction_.mode(0) == Mode::DataRegisterDirect) {
						perform_state_ = Perform_np;
					} else {
						select_flag_ = Microcycle::SelectByte;
						MoveToStateSpecific(TwoOp_Predec_bw);
					}
				})
				Duplicate(SUBXw, ADDXw)	StdCASE(ADDXw, {
					if(instruction_.mode(0) == Mode::DataRegisterDirect) {
						perform_state_ = Perform_np;
					} else {
						select_flag_ = Microcycle::SelectWord;
						MoveToStateSpecific(TwoOp_Predec_bw);
					}
				})
				Duplicate(SUBXl, ADDXl)	StdCASE(ADDXl, {
					if(instruction_.mode(0) == Mode::DataRegisterDirect) {
						perform_state_ = Perform_np_nn;
					} else {
						MoveToStateSpecific(TwoOp_Predec_l);
					}
				})

				StdCASE(Scc, {
					if(instruction_.mode(0) == Mode::DataRegisterDirect) {
						perform_state_ = Scc_Dn;
					} else {
						perform_state_ = Perform_np;
					}
				});

				SpecialCASE(DBcc);

				SpecialCASE(Bccb);
				SpecialCASE(Bccw);

				SpecialCASE(BSRb);
				SpecialCASE(BSRw);

				Duplicate(JMP, JSR)
				StdCASE(JSR, {
					post_ea_state_ =
						(instruction_.operation == InstructionSet::M68k::Operation::JSR) ?
							JSR : JMP;

					switch(instruction_.mode(0)) {
						case Mode::AddressRegisterIndirect:
							MoveToStateSpecific(JSRJMPAddressRegisterIndirect);
						case Mode::AddressRegisterIndirectWithDisplacement:
							MoveToStateSpecific(JSRJMPAddressRegisterIndirectWithDisplacement);
						case Mode::AddressRegisterIndirectWithIndex8bitDisplacement:
							MoveToStateSpecific(JSRJMPAddressRegisterIndirectWithIndex8bitDisplacement);
						case Mode::ProgramCounterIndirectWithDisplacement:
							MoveToStateSpecific(JSRJMPProgramCounterIndirectWithDisplacement);
						case Mode::ProgramCounterIndirectWithIndex8bitDisplacement:
							MoveToStateSpecific(JSRJMPProgramCounterIndirectWithIndex8bitDisplacement);
						case Mode::AbsoluteShort:
							MoveToStateSpecific(JSRJMPAbsoluteShort);
						case Mode::AbsoluteLong:
							MoveToStateSpecific(JSRJMPAbsoluteLong);

						default: assert(false);
					}
				});

				StdCASE(BTST, {
					switch(instruction_.mode(1)) {
						default:
							perform_state_ = Perform_np;
						break;
						case Mode::DataRegisterDirect:
						case Mode::ImmediateData:
							perform_state_ = Perform_np_n;
						break;
					}
				});

				Duplicate(BCHG, BSET)
				StdCASE(BSET, {
					switch(instruction_.mode(1)) {
						default:
							perform_state_ = Perform_np;
						break;
						case Mode::DataRegisterDirect:
						case Mode::ImmediateData:
							perform_state_ = BCHG_BSET_Dn;
						break;
					}
				});

				StdCASE(BCLR, {
					switch(instruction_.mode(1)) {
						default:
							perform_state_ = Perform_np;
						break;
						case Mode::DataRegisterDirect:
						case Mode::ImmediateData:
							perform_state_ = BCLR_Dn;
						break;
					}
				});

				StdCASE(MOVEPl, {
					if(instruction_.mode(0) == Mode::DataRegisterDirect) {
						MoveToStateSpecific(MOVEPtoM_l);
					} else {
						MoveToStateSpecific(MOVEPtoR_l);
					}
				});

				StdCASE(MOVEPw, {
					if(instruction_.mode(0) == Mode::DataRegisterDirect) {
						MoveToStateSpecific(MOVEPtoM_w);
					} else {
						MoveToStateSpecific(MOVEPtoR_w);
					}
				});

				Duplicate(ORItoCCR, EORItoCCR);	Duplicate(ANDItoCCR, EORItoCCR);
				StdCASE(EORItoCCR, 	perform_state_ = LogicalToSR);

				Duplicate(ORItoSR, EORItoSR);	Duplicate(ANDItoSR, EORItoSR);
				StdCASE(EORItoSR, 	perform_state_ = LogicalToSR);

				StdCASE(MOVEMtoRl,	perform_state_ = MOVEMtoR);
				StdCASE(MOVEMtoRw,	perform_state_ = MOVEMtoR);
				StdCASE(MOVEMtoMl,	perform_state_ = MOVEMtoM);
				StdCASE(MOVEMtoMw,	perform_state_ = MOVEMtoM);

				StdCASE(TSTb,		perform_state_ = Perform_np);
				StdCASE(TSTw,		perform_state_ = Perform_np);
				StdCASE(TSTl,		perform_state_ = Perform_np);

				StdCASE(DIVU,		perform_state_ = DIVU_DIVS);
				StdCASE(DIVS,		perform_state_ = DIVU_DIVS);
				StdCASE(MULU,		perform_state_ = Perform_idle_dyamic_Dn);
				StdCASE(MULS,		perform_state_ = Perform_idle_dyamic_Dn);

				StdCASE(LEA, {
					post_ea_state_ = LEA;
					MoveToStateSpecific(CalcEffectiveAddressIdleFor8bitDisplacement);
				});
				StdCASE(PEA, {
					post_ea_state_ = PEA;
					MoveToStateSpecific(CalcEffectiveAddressIdleFor8bitDisplacement);
				});

				StdCASE(TAS, {
					// TAS uses a special atomic bus cycle for memory accesses,
					// but is also available as DataRegisterDirect, with no
					// memory access whatsoever. So segue elsewhere here only
					// for the other cases.
					if(instruction_.mode(0) != Mode::DataRegisterDirect) {
						post_ea_state_ = TAS;
						MoveToStateSpecific(CalcEffectiveAddressIdleFor8bitDisplacement);
					}

					perform_state_ = Perform_np;
				});

				StdCASE(MOVEtoCCR, 	perform_state_ = MOVEtoCCRSR);
				StdCASE(MOVEtoSR, 	perform_state_ = MOVEtoCCRSR);
				StdCASE(MOVEfromSR, {
					if(instruction_.mode(0) == Mode::DataRegisterDirect) {
						perform_state_ = Perform_np_n;
					} else {
						perform_state_ = Perform_np;
					}
				});

				SpecialCASE(RTR);
				SpecialCASE(RTE);
				SpecialCASE(RTS);

#define ShiftGroup(suffix, state)								\
				Duplicate(ASL##suffix, ASR##suffix);			\
				Duplicate(LSL##suffix, ASR##suffix);			\
				Duplicate(LSR##suffix, ASR##suffix);			\
				Duplicate(ROL##suffix, ASR##suffix);			\
				Duplicate(ROR##suffix, ASR##suffix);			\
				Duplicate(ROXL##suffix, ASR##suffix);			\
				Duplicate(ROXR##suffix, ASR##suffix);			\
				StdCASE(ASR##suffix, perform_state_ = state );

				ShiftGroup(m, Perform_np)
				ShiftGroup(b, Perform_idle_dyamic_Dn)
				ShiftGroup(w, Perform_idle_dyamic_Dn)
				ShiftGroup(l, Perform_idle_dyamic_Dn)
#undef ShiftGroup

				SpecialCASE(LINKw);
				SpecialCASE(UNLINK);

				SpecialCASE(RESET);
				SpecialCASE(NOP);

				StdCASE(MOVEtoUSP, perform_state_ = Perform_np);
				StdCASE(MOVEfromUSP, perform_state_ = Perform_np);

				SpecialCASE(STOP);

				SpecialCASE(TRAP);
				SpecialCASE(TRAPV);

				default:
					assert(false);
			}

#undef Duplicate
#undef StdCASE
#undef CASE
#undef SpecialCASE
#undef CheckSupervisor

	// MARK: - Fetch, dispatch.

#define MoveToNextOperand(x)				\
	++next_operand_;						\
	if(next_operand_ == 2) {				\
		MoveToStateDynamic(perform_state_);	\
	}										\
	MoveToStateSpecific(x)

		// Check the operand flags to determine whether the byte or word
		// operand at index next_operand_ needs to be fetched, and if so
		// then calculate the EA and do so.
		BeginState(FetchOperand_bw):
			// Check that this operand is meant to be fetched; if not then either:
			//
			//	(i) this operand isn't used; or
			//	(ii) its address calculation will end up conflated with performance,
			//		so there's no generic bus-accurate approach.
			if(!(operand_flags_ & (1 << next_operand_))) {
				MoveToStateDynamic(perform_state_);
			}

			// Figure out how to fetch it.
			switch(instruction_.mode(next_operand_)) {
				case Mode::AddressRegisterDirect:
				case Mode::DataRegisterDirect:
					operand_[next_operand_] = registers_[instruction_.lreg(next_operand_)];
				MoveToNextOperand(FetchOperand_bw);

				case Mode::Quick:
					operand_[next_operand_].l = InstructionSet::M68k::quick(opcode_, instruction_.operation);
				MoveToNextOperand(FetchOperand_bw);

				case Mode::AddressRegisterIndirect:
					MoveToStateSpecific(FetchAddressRegisterIndirect_bw);
				case Mode::AddressRegisterIndirectWithPostincrement:
					MoveToStateSpecific(FetchAddressRegisterIndirectWithPostincrement_bw);
				case Mode::AddressRegisterIndirectWithPredecrement:
					MoveToStateSpecific(FetchAddressRegisterIndirectWithPredecrement_bw);
				case Mode::AddressRegisterIndirectWithDisplacement:
					MoveToStateSpecific(FetchAddressRegisterIndirectWithDisplacement_bw);
				case Mode::AddressRegisterIndirectWithIndex8bitDisplacement:
					MoveToStateSpecific(FetchAddressRegisterIndirectWithIndex8bitDisplacement_bw);
				case Mode::ProgramCounterIndirectWithDisplacement:
					MoveToStateSpecific(FetchProgramCounterIndirectWithDisplacement_bw);
				case Mode::ProgramCounterIndirectWithIndex8bitDisplacement:
					MoveToStateSpecific(FetchProgramCounterIndirectWithIndex8bitDisplacement_bw);
				case Mode::AbsoluteShort:
					MoveToStateSpecific(FetchAbsoluteShort_bw);
				case Mode::AbsoluteLong:
					MoveToStateSpecific(FetchAbsoluteLong_bw);
				case Mode::ImmediateData:
					MoveToStateSpecific(FetchImmediateData_bw);

				// Should be impossible to reach.
				default:
					assert(false);
			}
		break;

		// As above, but for .l.
		BeginState(FetchOperand_l):
			if(!(operand_flags_ & (1 << next_operand_))) {
				MoveToStateDynamic(perform_state_);
			}

			switch(instruction_.mode(next_operand_)) {
				case Mode::AddressRegisterDirect:
				case Mode::DataRegisterDirect:
					operand_[next_operand_] = registers_[instruction_.lreg(next_operand_)];
				MoveToNextOperand(FetchOperand_l);

				case Mode::Quick:
					operand_[next_operand_].l = InstructionSet::M68k::quick(opcode_, instruction_.operation);
				MoveToNextOperand(FetchOperand_l);

				case Mode::AddressRegisterIndirect:
					MoveToStateSpecific(FetchAddressRegisterIndirect_l);
				case Mode::AddressRegisterIndirectWithPostincrement:
					MoveToStateSpecific(FetchAddressRegisterIndirectWithPostincrement_l);
				case Mode::AddressRegisterIndirectWithPredecrement:
					MoveToStateSpecific(FetchAddressRegisterIndirectWithPredecrement_l);
				case Mode::AddressRegisterIndirectWithDisplacement:
					MoveToStateSpecific(FetchAddressRegisterIndirectWithDisplacement_l);
				case Mode::AddressRegisterIndirectWithIndex8bitDisplacement:
					MoveToStateSpecific(FetchAddressRegisterIndirectWithIndex8bitDisplacement_l);
				case Mode::ProgramCounterIndirectWithDisplacement:
					MoveToStateSpecific(FetchProgramCounterIndirectWithDisplacement_l);
				case Mode::ProgramCounterIndirectWithIndex8bitDisplacement:
					MoveToStateSpecific(FetchProgramCounterIndirectWithIndex8bitDisplacement_l);
				case Mode::AbsoluteShort:
					MoveToStateSpecific(FetchAbsoluteShort_l);
				case Mode::AbsoluteLong:
					MoveToStateSpecific(FetchAbsoluteLong_l);
				case Mode::ImmediateData:
					MoveToStateSpecific(FetchImmediateData_l);

				// Should be impossible to reach.
				default:
					assert(false);
			}
		break;

		BeginState(CalcEffectiveAddressIdleFor8bitDisplacement):
			if(
				instruction_.mode(next_operand_) != Mode::AddressRegisterIndirectWithIndex8bitDisplacement &&
				instruction_.mode(next_operand_) != Mode::ProgramCounterIndirectWithIndex8bitDisplacement
			) {
				MoveToStateSpecific(CalcEffectiveAddress);
			}

			IdleBus(1);
			[[fallthrough]];

		BeginState(CalcEffectiveAddress):
			switch(instruction_.mode(next_operand_)) {
				default:
					MoveToStateDynamic(post_ea_state_);

				case Mode::AddressRegisterIndirect:
					MoveToStateSpecific(CalcAddressRegisterIndirect);
				case Mode::AddressRegisterIndirectWithPostincrement:
					MoveToStateSpecific(CalcAddressRegisterIndirectWithPostincrement);
				case Mode::AddressRegisterIndirectWithPredecrement:
					MoveToStateSpecific(CalcAddressRegisterIndirectWithPredecrement);
				case Mode::AddressRegisterIndirectWithDisplacement:
					MoveToStateSpecific(CalcAddressRegisterIndirectWithDisplacement);
				case Mode::AddressRegisterIndirectWithIndex8bitDisplacement:
					MoveToStateSpecific(CalcAddressRegisterIndirectWithIndex8bitDisplacement);
				case Mode::ProgramCounterIndirectWithDisplacement:
					MoveToStateSpecific(CalcProgramCounterIndirectWithDisplacement);
				case Mode::ProgramCounterIndirectWithIndex8bitDisplacement:
					MoveToStateSpecific(CalcProgramCounterIndirectWithIndex8bitDisplacement);
				case Mode::AbsoluteShort:
					MoveToStateSpecific(CalcAbsoluteShort);
				case Mode::AbsoluteLong:
					MoveToStateSpecific(CalcAbsoluteLong);
			}

	// MARK: - Fetch, addressing modes.

		//
		// AddressRegisterIndirect
		//
		BeginState(FetchAddressRegisterIndirect_bw):
			effective_address_[next_operand_].l = registers_[8 + instruction_.reg(next_operand_)].l;
			SetDataAddress(effective_address_[next_operand_].l);

			Access(operand_[next_operand_].low);	// nr
		MoveToNextOperand(FetchOperand_bw);

		BeginState(FetchAddressRegisterIndirect_l):
			effective_address_[next_operand_].l = registers_[8 + instruction_.reg(next_operand_)].l;
			SetDataAddress(effective_address_[next_operand_].l);

			Access(operand_[next_operand_].high);	// nR

			effective_address_[next_operand_].l += 2;
			Access(operand_[next_operand_].low);	// nr
		MoveToNextOperand(FetchOperand_l);

		BeginState(CalcAddressRegisterIndirect):
			effective_address_[next_operand_].l = registers_[8 + instruction_.reg(next_operand_)].l;
		MoveToStateDynamic(post_ea_state_);

		BeginState(JSRJMPAddressRegisterIndirect):
			effective_address_[0].l = registers_[8 + instruction_.reg(next_operand_)].l;
			temporary_address_.l = instruction_address_.l + 2;
		MoveToStateDynamic(post_ea_state_);

		//
		// AddressRegisterIndirectWithPostincrement
		//
		BeginState(FetchAddressRegisterIndirectWithPostincrement_bw):
			effective_address_[next_operand_].l = registers_[8 + instruction_.reg(next_operand_)].l;
			registers_[8 + instruction_.reg(next_operand_)].l +=
				address_increments[int(instruction_.operand_size())][instruction_.reg(next_operand_)];

			SetDataAddress(effective_address_[next_operand_].l);
			Access(operand_[next_operand_].low);	// nr
		MoveToNextOperand(FetchOperand_bw);

		BeginState(FetchAddressRegisterIndirectWithPostincrement_l):
			effective_address_[next_operand_].l = registers_[8 + instruction_.reg(next_operand_)].l;
			registers_[8 + instruction_.reg(next_operand_)].l += 4;

			SetDataAddress(effective_address_[next_operand_].l);
			Access(operand_[next_operand_].high);	// nR
			effective_address_[next_operand_].l += 2;
			Access(operand_[next_operand_].low);	// nr
		MoveToNextOperand(FetchOperand_l);

		BeginState(CalcAddressRegisterIndirectWithPostincrement):
			effective_address_[next_operand_].l = registers_[8 + instruction_.reg(next_operand_)].l;
			registers_[8 + instruction_.reg(next_operand_)].l +=
				address_increments[int(instruction_.operand_size())][instruction_.reg(next_operand_)];
		MoveToStateDynamic(post_ea_state_);

		//
		// AddressRegisterIndirectWithPredecrement
		//
		BeginState(FetchAddressRegisterIndirectWithPredecrement_bw):
			registers_[8 + instruction_.reg(next_operand_)].l -=
				address_increments[int(instruction_.operand_size())][instruction_.reg(next_operand_)];
			effective_address_[next_operand_].l = registers_[8 + instruction_.reg(next_operand_)].l;
			SetDataAddress(effective_address_[next_operand_].l);

			IdleBus(1);								// n
			Access(operand_[next_operand_].low);	// nr
		MoveToNextOperand(FetchOperand_bw);

		BeginState(FetchAddressRegisterIndirectWithPredecrement_l):
			registers_[8 + instruction_.reg(next_operand_)].l -= 4;
			effective_address_[next_operand_].l = registers_[8 + instruction_.reg(next_operand_)].l;
			SetDataAddress(effective_address_[next_operand_].l);

			IdleBus(1);								// n
			Access(operand_[next_operand_].high);	// nR
			effective_address_[next_operand_].l += 2;
			Access(operand_[next_operand_].low);	// nr
		MoveToNextOperand(FetchOperand_l);

		BeginState(CalcAddressRegisterIndirectWithPredecrement):
			registers_[8 + instruction_.reg(next_operand_)].l -= address_increments[int(instruction_.operand_size())][instruction_.reg(next_operand_)];
			effective_address_[next_operand_].l = registers_[8 + instruction_.reg(next_operand_)].l;
		MoveToStateDynamic(post_ea_state_);

		//
		// AddressRegisterIndirectWithDisplacement
		//
		BeginState(FetchAddressRegisterIndirectWithDisplacement_bw):
			effective_address_[next_operand_].l =
				registers_[8 + instruction_.reg(next_operand_)].l +
				uint32_t(int16_t(prefetch_.w));
			SetDataAddress(effective_address_[next_operand_].l);

			Prefetch();								// np
			Access(operand_[next_operand_].low);	// nr
		MoveToNextOperand(FetchOperand_bw);

		BeginState(FetchAddressRegisterIndirectWithDisplacement_l):
			effective_address_[next_operand_].l =
				registers_[8 + instruction_.reg(next_operand_)].l +
				uint32_t(int16_t(prefetch_.w));
			SetDataAddress(effective_address_[next_operand_].l);

			Prefetch();								// np
			Access(operand_[next_operand_].high);	// nR
			effective_address_[next_operand_].l += 2;
			Access(operand_[next_operand_].low);	// nr
		MoveToNextOperand(FetchOperand_l);

		BeginState(CalcAddressRegisterIndirectWithDisplacement):
			effective_address_[next_operand_].l =
				registers_[8 + instruction_.reg(next_operand_)].l +
				uint32_t(int16_t(prefetch_.w));
			Prefetch();								// np
		MoveToStateDynamic(post_ea_state_);

		BeginState(JSRJMPAddressRegisterIndirectWithDisplacement):
			effective_address_[0].l =
				registers_[8 + instruction_.reg(next_operand_)].l +
				uint32_t(int16_t(prefetch_.w));
			IdleBus(1);								// n
			temporary_address_.l = instruction_address_.l + 4;
		MoveToStateDynamic(post_ea_state_);

		//
		// ProgramCounterIndirectWithDisplacement
		//
		BeginState(FetchProgramCounterIndirectWithDisplacement_bw):
			effective_address_[next_operand_].l =
				program_counter_.l - 2 +
				uint32_t(int16_t(prefetch_.w));
			SetDataAddress(effective_address_[next_operand_].l);

			Prefetch();								// np
			Access(operand_[next_operand_].low);	// nr
		MoveToNextOperand(FetchOperand_bw);

		BeginState(FetchProgramCounterIndirectWithDisplacement_l):
			effective_address_[next_operand_].l =
				program_counter_.l - 2 +
				uint32_t(int16_t(prefetch_.w));
			SetDataAddress(effective_address_[next_operand_].l);

			Prefetch();								// np
			Access(operand_[next_operand_].high);	// nR
			effective_address_[next_operand_].l += 2;
			Access(operand_[next_operand_].low);	// nr
		MoveToNextOperand(FetchOperand_l);

		BeginState(CalcProgramCounterIndirectWithDisplacement):
			effective_address_[next_operand_].l =
				program_counter_.l - 2 +
				uint32_t(int16_t(prefetch_.w));
			Prefetch();								// np
		MoveToStateDynamic(post_ea_state_);

		BeginState(JSRJMPProgramCounterIndirectWithDisplacement):
			effective_address_[0].l =
				program_counter_.l - 2 +
				uint32_t(int16_t(prefetch_.w));
			IdleBus(1);								// n
			temporary_address_.l = instruction_address_.l + 4;
		MoveToStateDynamic(post_ea_state_);

		//
		// AddressRegisterIndirectWithIndex8bitDisplacement
		//
#define d8Xn(base)												\
	base +														\
	((prefetch_.w & 0x800) ?									\
		registers_[prefetch_.w >> 12].l :						\
		uint32_t(int16_t(registers_[prefetch_.w >> 12].w))) +	\
	uint32_t(int8_t(prefetch_.b));

		BeginState(FetchAddressRegisterIndirectWithIndex8bitDisplacement_bw):
			effective_address_[next_operand_].l = d8Xn(registers_[8 + instruction_.reg(next_operand_)].l);
			SetDataAddress(effective_address_[next_operand_].l);

			IdleBus(1);								// n
			Prefetch();								// np
			Access(operand_[next_operand_].low);	// nr
		MoveToNextOperand(FetchOperand_bw);

		BeginState(FetchAddressRegisterIndirectWithIndex8bitDisplacement_l):
			effective_address_[next_operand_].l = d8Xn(registers_[8 + instruction_.reg(next_operand_)].l);
			SetDataAddress(effective_address_[next_operand_].l);

			IdleBus(1);								// n
			Prefetch();								// np
			Access(operand_[next_operand_].high);	// nR
			effective_address_[next_operand_].l += 2;
			Access(operand_[next_operand_].low);	// nr
		MoveToNextOperand(FetchOperand_l);

		BeginState(CalcAddressRegisterIndirectWithIndex8bitDisplacement):
			effective_address_[next_operand_].l = d8Xn(registers_[8 + instruction_.reg(next_operand_)].l);
			Prefetch();								// np
			IdleBus(1);								// n
		MoveToStateDynamic(post_ea_state_);

		BeginState(JSRJMPAddressRegisterIndirectWithIndex8bitDisplacement):
			effective_address_[0].l = d8Xn(registers_[8 + instruction_.reg(next_operand_)].l);
			IdleBus(3);								// n nn
			temporary_address_.l = instruction_address_.l + 4;
		MoveToStateDynamic(post_ea_state_);

		//
		// ProgramCounterIndirectWithIndex8bitDisplacement
		//
		BeginState(FetchProgramCounterIndirectWithIndex8bitDisplacement_bw):
			effective_address_[next_operand_].l = d8Xn(program_counter_.l - 2);
			SetDataAddress(effective_address_[next_operand_].l);

			IdleBus(1);								// n
			Prefetch();								// np
			Access(operand_[next_operand_].low);	// nr
		MoveToNextOperand(FetchOperand_bw);

		BeginState(FetchProgramCounterIndirectWithIndex8bitDisplacement_l):
			effective_address_[next_operand_].l = d8Xn(program_counter_.l - 2);
			SetDataAddress(effective_address_[next_operand_].l);

			IdleBus(1);								// n
			Prefetch();								// np
			Access(operand_[next_operand_].high);	// nR
			effective_address_[next_operand_].l += 2;
			Access(operand_[next_operand_].low);	// nr
		MoveToNextOperand(FetchOperand_l);

		BeginState(CalcProgramCounterIndirectWithIndex8bitDisplacement):
			effective_address_[next_operand_].l = d8Xn(program_counter_.l - 2);
			Prefetch();								// np
			IdleBus(1);								// n
		MoveToStateDynamic(post_ea_state_);

		BeginState(JSRJMPProgramCounterIndirectWithIndex8bitDisplacement):
			effective_address_[0].l = d8Xn(program_counter_.l - 2);
			IdleBus(3);								// n nn
			temporary_address_.l = instruction_address_.l + 4;
		MoveToStateDynamic(post_ea_state_);

#undef d8Xn

		//
		// AbsoluteShort
		//
		BeginState(FetchAbsoluteShort_bw):
			effective_address_[next_operand_].l = uint32_t(int16_t(prefetch_.w));
			SetDataAddress(effective_address_[next_operand_].l);

			Prefetch();								// np
			Access(operand_[next_operand_].low);	// nr
		MoveToNextOperand(FetchOperand_bw);

		BeginState(FetchAbsoluteShort_l):
			effective_address_[next_operand_].l = uint32_t(int16_t(prefetch_.w));
			SetDataAddress(effective_address_[next_operand_].l);

			Prefetch();								// np
			Access(operand_[next_operand_].high);	// nR
			effective_address_[next_operand_].l += 2;
			Access(operand_[next_operand_].low);	// nr
		MoveToNextOperand(FetchOperand_l);

		BeginState(CalcAbsoluteShort):
			effective_address_[next_operand_].l = uint32_t(int16_t(prefetch_.w));
			Prefetch();								// np
		MoveToStateDynamic(post_ea_state_);

		BeginState(JSRJMPAbsoluteShort):
			effective_address_[0].l = uint32_t(int16_t(prefetch_.w));
			IdleBus(1);								// n
			temporary_address_.l = instruction_address_.l + 4;
		MoveToStateDynamic(post_ea_state_);

		//
		// AbsoluteLong
		//
		BeginState(FetchAbsoluteLong_bw):
			Prefetch();								// np

			effective_address_[next_operand_].l = prefetch_.l;
			SetDataAddress(effective_address_[next_operand_].l);

			Prefetch();								// np
			Access(operand_[next_operand_].low);	// nr
		MoveToNextOperand(FetchOperand_bw);

		BeginState(FetchAbsoluteLong_l):
			Prefetch();								// np

			effective_address_[next_operand_].l = prefetch_.l;
			SetDataAddress(effective_address_[next_operand_].l);

			Prefetch();								// np
			Access(operand_[next_operand_].high);	// nR
			effective_address_[next_operand_].l += 2;
			Access(operand_[next_operand_].low);	// nr
		MoveToNextOperand(FetchOperand_l);

		BeginState(CalcAbsoluteLong):
			Prefetch();								// np
			effective_address_[next_operand_].l = prefetch_.l;
			Prefetch();								// np
		MoveToStateDynamic(post_ea_state_);

		BeginState(JSRJMPAbsoluteLong):
			Prefetch();								// np
			effective_address_[0].l = prefetch_.l;
			temporary_address_.l = instruction_address_.l + 6;
		MoveToStateDynamic(post_ea_state_);

		//
		// ImmediateData
		//
		BeginState(FetchImmediateData_bw):
			operand_[next_operand_].w = prefetch_.w;
			Prefetch();								// np
		MoveToNextOperand(FetchOperand_bw);

		BeginState(FetchImmediateData_l):
			Prefetch();								// np
			operand_[next_operand_].l = prefetch_.l;
			Prefetch();								// np
		MoveToNextOperand(FetchOperand_l);

#undef MoveToNextOperand

	// MARK: - Store.

#define MoveToNextOperand(x)			\
	++next_operand_;					\
	if(next_operand_ == 2) {			\
		MoveToStateSpecific(Decode);	\
	}									\
	MoveToStateSpecific(x)

		// Store operand is a lot simpler: only one operand is ever stored, and its address
		// is already known. So this can either skip straight back to ::Decode if the target
		// is a register, otherwise a single write operation can occur.
		BeginState(StoreOperand):
			switch(instruction_.operand_size()) {
				case InstructionSet::M68k::DataSize::LongWord:
					SetupDataAccess(0, Microcycle::SelectWord);
				MoveToStateSpecific(StoreOperand_l);

				case InstructionSet::M68k::DataSize::Word:
					SetupDataAccess(0, Microcycle::SelectWord);
				MoveToStateSpecific(StoreOperand_bw);

				case InstructionSet::M68k::DataSize::Byte:
					SetupDataAccess(0, Microcycle::SelectByte);
				MoveToStateSpecific(StoreOperand_bw);
			}

		BeginState(StoreOperand_bw):
			if(!(operand_flags_ & 0x4 << next_operand_)) {
				MoveToNextOperand(StoreOperand_bw);
			}

			switch(instruction_.mode(next_operand_)) {
				// Data register: write only the part of the word that has changed.
				case Mode::DataRegisterDirect: {
					const uint32_t write_mask = size_masks[int(instruction_.operand_size())];
					const int reg = instruction_.reg(next_operand_);

					registers_[reg].l =
						(operand_[next_operand_].l & write_mask) |
						(registers_[reg].l & ~write_mask);
				}
				MoveToNextOperand(StoreOperand_bw);

				// Address register: always rewrite the whole word; the smaller
				// result will have been sign extended.
				case Mode::AddressRegisterDirect:
					registers_[instruction_.lreg(next_operand_)] = operand_[next_operand_];
				MoveToNextOperand(StoreOperand_bw);

				default: break;
			}

			SetDataAddress(effective_address_[next_operand_].l);
			Access(operand_[next_operand_].low);		// nw
		MoveToNextOperand(StoreOperand_bw);

		BeginState(StoreOperand_l):
			if(!(operand_flags_ & 0x4 << next_operand_)) {
				MoveToNextOperand(StoreOperand_l);
			}

			if(instruction_.mode(next_operand_) <= Mode::AddressRegisterDirect) {
				registers_[instruction_.lreg(next_operand_)] = operand_[next_operand_];
				MoveToNextOperand(StoreOperand_l);
			}

			SetupDataAccess(0, Microcycle::SelectWord);
			SetDataAddress(effective_address_[next_operand_].l);
			Access(operand_[next_operand_].low);		// nw

			effective_address_[next_operand_].l -= 2;
			Access(operand_[next_operand_].high);		// nW
		MoveToNextOperand(StoreOperand_l);

#define PerformDynamic()	\
	InstructionSet::M68k::perform<InstructionSet::M68k::Model::M68000>(	\
		instruction_, operand_[0], operand_[1], status_, *static_cast<ProcessorBase *>(this));

#define PerformSpecific(x)	\
	InstructionSet::M68k::perform<	\
		InstructionSet::M68k::Model::M68000,	\
		ProcessorBase,	\
		InstructionSet::M68k::Operation::x	\
	>(	\
		instruction_, operand_[0], operand_[1], status_, *static_cast<ProcessorBase *>(this));

		//
		// Various generic forms of perform.
		//
#define MoveToWritePhase()					\
	if(operand_flags_ & 0x0c) {				\
		next_operand_ = 0;					\
		MoveToStateSpecific(StoreOperand);	\
	} else {								\
		MoveToStateSpecific(Decode);		\
	}

		BeginState(Perform_np):
			PerformDynamic();
			Prefetch();			// np
		MoveToWritePhase();

		BeginState(Perform_np_n):
			PerformDynamic();
			Prefetch();			// np
			IdleBus(1);			// n
		MoveToWritePhase();

		BeginState(Perform_np_nn):
			PerformDynamic();
			Prefetch();			// np
			IdleBus(2);			// nn
		MoveToWritePhase();

#undef MoveToWritePhase


		//
		// Specific forms of perform...
		//

		BeginState(MOVE):
			PerformDynamic();

			// In all cases except predecrement mode: do the usual address
			// calculate and storage, then do the next prefetch and decode.
			//
			// In predecrement mode: do the prefetch, then write the result.
			//
			// For here, lump data and address register direct in with predec,
			// so that all that's left is modes that write to memory and then
			// prefetch.
			switch(instruction_.mode(1)) {
				case Mode::DataRegisterDirect: {
					const uint32_t write_mask = size_masks[int(instruction_.operand_size())];
					const int reg = instruction_.reg(1);

					registers_[reg].l =
						(operand_[1].l & write_mask) |
						(registers_[reg].l & ~write_mask);
				}
				MoveToStateSpecific(MOVE_prefetch_decode);

				case Mode::AddressRegisterDirect:
					registers_[8 + instruction_.reg(1)].l = operand_[1].l;
				MoveToStateSpecific(MOVE_prefetch_decode);

				case Mode::AddressRegisterIndirectWithPredecrement:
				MoveToStateSpecific(MOVE_predec);

				default: break;
			}

			next_operand_ = 1;
			post_ea_state_ = MOVE_complete;
		MoveToStateSpecific(CalcEffectiveAddress);

		BeginState(MOVE_prefetch_decode):
			Prefetch();
		MoveToStateSpecific(Decode);

		BeginState(MOVE_predec):
			Prefetch();

			SetDataAddress(registers_[8 + instruction_.reg(1)].l);
			switch(instruction_.operand_size()) {
				case InstructionSet::M68k::DataSize::LongWord:
				MoveToStateSpecific(MOVE_predec_l);

				case InstructionSet::M68k::DataSize::Word:
					SetupDataAccess(0, Microcycle::SelectWord);
					registers_[8 + instruction_.reg(1)].l -= 2;
				break;

				case InstructionSet::M68k::DataSize::Byte:
					SetupDataAccess(0, Microcycle::SelectByte);
					registers_[8 + instruction_.reg(1)].l -=
						address_increments[0][instruction_.reg(next_operand_)];
				break;
			}

			SetDataAddress(registers_[8 + instruction_.reg(1)].l);
			Access(operand_[1].low);
		MoveToStateSpecific(Decode);

		BeginState(MOVE_predec_l):
			SetupDataAccess(0, Microcycle::SelectWord);

			registers_[8 + instruction_.reg(1)].l -= 2;
			Access(operand_[1].low);
			registers_[8 + instruction_.reg(1)].l -= 2;
			Access(operand_[1].high);
		MoveToStateSpecific(Decode);

		BeginState(MOVE_complete):
			SetDataAddress(effective_address_[1].l);

			switch(instruction_.operand_size()) {
				case InstructionSet::M68k::DataSize::LongWord:
					SetupDataAccess(0, Microcycle::SelectWord);
				MoveToStateSpecific(MOVE_complete_l);

				case InstructionSet::M68k::DataSize::Word:
					SetupDataAccess(0, Microcycle::SelectWord);
				break;

				case InstructionSet::M68k::DataSize::Byte:
					SetupDataAccess(0, Microcycle::SelectByte);
				break;
			}

			Access(operand_[1].low);
			Prefetch();
		MoveToStateSpecific(Decode);

		BeginState(MOVE_complete_l):
			Access(operand_[1].high);
			effective_address_[1].l += 2;
			Access(operand_[1].low);
			Prefetch();
		MoveToStateSpecific(Decode);

		//
		// [ABCD/SBCD/SUBX/ADDX] (An)-, (An)-
		//
		BeginState(TwoOp_Predec_bw):
			IdleBus(1);					// n

			SetupDataAccess(Microcycle::Read, select_flag_);

			SetDataAddress(registers_[8 + instruction_.reg(0)].l);
			registers_[8 + instruction_.reg(0)].l -= address_increments[int(instruction_.operand_size())][instruction_.reg(0)];
			Access(operand_[0].low);	// nr

			SetDataAddress(registers_[8 + instruction_.reg(1)].l);
			registers_[8 + instruction_.reg(1)].l -= address_increments[int(instruction_.operand_size())][instruction_.reg(1)];
			Access(operand_[1].low);	// nr

			Prefetch();					// np

			PerformDynamic();

			SetupDataAccess(0, select_flag_);
			Access(operand_[1].low);	// nw
		MoveToStateSpecific(Decode);

		BeginState(TwoOp_Predec_l):
			IdleBus(1);					// n

			SetupDataAccess(Microcycle::Read, Microcycle::SelectWord);

			SetDataAddress(registers_[8 + instruction_.reg(0)].l);
			registers_[8 + instruction_.reg(0)].l -= 2;
			Access(operand_[0].low);	// nr

			registers_[8 + instruction_.reg(0)].l -= 2;
			Access(operand_[0].high);	// nR

			SetDataAddress(registers_[8 + instruction_.reg(1)].l);
			registers_[8 + instruction_.reg(1)].l -= 2;
			Access(operand_[1].low);	// nr

			registers_[8 + instruction_.reg(1)].l -= 2;
			Access(operand_[1].high);	// nR

			PerformDynamic();

			SetupDataAccess(0, Microcycle::SelectWord);

			registers_[8 + instruction_.reg(1)].l += 2;
			Access(operand_[1].low);	// nw

			Prefetch();					// np

			registers_[8 + instruction_.reg(1)].l -= 2;
			Access(operand_[1].high);	// nW
		MoveToStateSpecific(Decode);

		//
		// CHK
		//
		BeginState(CHK):
			Prefetch();			// np
			PerformSpecific(CHK);

			// Proper next state will have been set by the flow controller
			// call-in; just allow dispatch to whatever it was.
		break;

		BeginState(CHK_no_trap):
			IdleBus(3);			// nn n
		MoveToStateSpecific(Decode);

		BeginState(CHK_was_over):
			IdleBus(2);			// nn
			ReloadInstructionAddress();
		RaiseException(InstructionSet::M68k::Exception::CHK);

		BeginState(CHK_was_under):
			IdleBus(3);			// n nn
			ReloadInstructionAddress();
		RaiseException(InstructionSet::M68k::Exception::CHK);

		//
		// Scc
		//
		BeginState(Scc_Dn):
			Prefetch();			// np
			PerformSpecific(Scc);

			// Next state will be set by did_scc.
		break;

		BeginState(Scc_Dn_did_set):
			IdleBus(1);			// n
			[[fallthrough]];
		BeginState(Scc_Dn_did_not_set):
			next_operand_ = 0;
		MoveToStateSpecific(StoreOperand);

		//
		// DBcc
		//
		BeginState(DBcc):
			operand_[0] = registers_[instruction_.reg(0)];
			operand_[1].w = uint16_t(int16_t(prefetch_.w));
			PerformSpecific(DBcc);
			registers_[instruction_.reg(0)].w = operand_[0].w;

			// Next state was set by complete_dbcc.
		break;

		BeginState(DBcc_branch_taken):
			IdleBus(1);		// n
			Prefetch();		// np
			Prefetch();		// np
		MoveToStateSpecific(Decode);

		BeginState(DBcc_condition_true):
			IdleBus(2);		// n n
			Prefetch();		// np
			Prefetch();		// np
		MoveToStateSpecific(Decode);

		BeginState(DBcc_counter_overflow):
			IdleBus(1);		// n

			// Yacht lists an extra np here; I'm assuming it's a read from where
			// the PC would have gone, had the branch been taken. So do that,
			// but then reset the PC to where it would have been.
			Prefetch();		// np

			program_counter_.l = instruction_address_.l + 4;
			Prefetch();		// np
			Prefetch();		// np
		MoveToStateSpecific(Decode);

		//
		// Bcc [.b and .w]
		//
		BeginState(Bccb):
			operand_[0].b = uint8_t(opcode_);
			PerformSpecific(Bccb);

			// Next state was set by complete_bcc.
		break;

		BeginState(Bccw):
			operand_[0].w = prefetch_.w;
			PerformSpecific(Bccw);

			// Next state was set by complete_bcc.
		break;

		BeginState(Bcc_branch_taken):
			IdleBus(1);		// n
			Prefetch();		// np
			Prefetch();		// np
		MoveToStateSpecific(Decode);

		BeginState(Bccb_branch_not_taken):
			IdleBus(2);		// nn
			Prefetch();		// np
		MoveToStateSpecific(Decode);

		BeginState(Bccw_branch_not_taken):
			IdleBus(2);		// nn
			Prefetch();		// np
			Prefetch();		// np
		MoveToStateSpecific(Decode);


#define Push(x)									\
	SetupDataAccess(0, Microcycle::SelectWord);	\
	SetDataAddress(registers_[15].l);			\
	registers_[15].l -= 4;						\
	Access(x.high);								\
	registers_[15].l += 2;						\
	Access(x.low);								\
	registers_[15].l -= 2;

#define Pop(x)													\
	SetupDataAccess(Microcycle::Read, Microcycle::SelectWord);	\
	SetDataAddress(registers_[15].l);							\
	Access(x.high);												\
	registers_[15].l += 2;										\
	Access(x.low);												\
	registers_[15].l += 2;

		//
		// BSR
		//
		BeginState(BSRb):
		BeginState(BSRw):
			IdleBus(1);		// n

			// Calculate the address of the next instruction and the next program counter.
			if(instruction_.operand_size() == InstructionSet::M68k::DataSize::Word) {
				temporary_address_.l = instruction_address_.l + 4;
				program_counter_.l = instruction_address_.l + uint32_t(int16_t(prefetch_.w)) + 2;
			} else {
				temporary_address_.l = instruction_address_.l + 2;
				program_counter_.l = instruction_address_.l + uint32_t(int8_t(opcode_)) + 2;
			}

			// Push the next instruction address to the stack.
			Push(temporary_address_);

			Prefetch();		// np
			Prefetch();		// np
		MoveToStateSpecific(Decode);

		//
		// JSR [push only; address calculation elsewhere], JMP
		//
		BeginState(JSR):
			// Update the program counter and prefetch once.
			program_counter_.l = effective_address_[0].l;
			Prefetch();		// np

			// Push the old PC onto the stack in upper, lower order.
			Push(temporary_address_);

			// Prefetch once more.
			Prefetch();
		MoveToStateSpecific(Decode);

		BeginState(JMP):
			// Update the program counter and prefetch once.
			program_counter_.l = effective_address_[0].l;
			Prefetch();		// np
			Prefetch();		// np
		MoveToStateSpecific(Decode);

		//
		// BSET, BCHG, BCLR
		//
		BeginState(BCHG_BSET_Dn):
			PerformDynamic();

			Prefetch();
			IdleBus(1 + dynamic_instruction_length_);
			registers_[instruction_.reg(1)] = operand_[1];
		MoveToStateSpecific(Decode);

		BeginState(BCLR_Dn):
			PerformSpecific(BCLR);

			Prefetch();
			IdleBus(2 + dynamic_instruction_length_);
			registers_[instruction_.reg(1)] = operand_[1];
		MoveToStateSpecific(Decode);

		//
		// MOVEP
		//
		BeginState(MOVEPtoM_l):
			temporary_address_.l = registers_[8 + instruction_.reg(1)].l + uint32_t(int16_t(prefetch_.w));
			SetDataAddress(temporary_address_.l);
			SetupDataAccess(0, Microcycle::SelectByte);

			Prefetch();						// np

			temporary_value_.b = uint8_t(registers_[instruction_.reg(0)].l >> 24);
			Access(temporary_value_.low);	// nW

			temporary_address_.l += 2;
			temporary_value_.b = uint8_t(registers_[instruction_.reg(0)].l >> 16);
			Access(temporary_value_.low);	// nW

			temporary_address_.l += 2;
			temporary_value_.b = uint8_t(registers_[instruction_.reg(0)].l >> 8);
			Access(temporary_value_.low);	// nw

			temporary_address_.l += 2;
			temporary_value_.b = uint8_t(registers_[instruction_.reg(0)].l);
			Access(temporary_value_.low);	// nw

			Prefetch();						// np
		MoveToStateSpecific(Decode);

		BeginState(MOVEPtoM_w):
			temporary_address_.l = registers_[8 + instruction_.reg(1)].l + uint32_t(int16_t(prefetch_.w));
			SetDataAddress(temporary_address_.l);
			SetupDataAccess(0, Microcycle::SelectByte);

			Prefetch();						// np

			temporary_value_.b = uint8_t(registers_[instruction_.reg(0)].l >> 8);
			Access(temporary_value_.low);	// nW

			temporary_address_.l += 2;
			temporary_value_.b = uint8_t(registers_[instruction_.reg(0)].l);
			Access(temporary_value_.low);	// nw

			Prefetch();						// np
		MoveToStateSpecific(Decode);

		BeginState(MOVEPtoR_l):
			temporary_address_.l = registers_[8 + instruction_.reg(0)].l + uint32_t(int16_t(prefetch_.w));
			SetDataAddress(temporary_address_.l);
			SetupDataAccess(Microcycle::Read, Microcycle::SelectByte);

			Prefetch();						// np

			Access(temporary_value_.low);	// nR
			registers_[instruction_.reg(1)].l = uint32_t(temporary_value_.b << 24);

			temporary_address_.l += 2;
			Access(temporary_value_.low);	// nR
			registers_[instruction_.reg(1)].l |= uint32_t(temporary_value_.b << 16);

			temporary_address_.l += 2;
			Access(temporary_value_.low);	// nr
			registers_[instruction_.reg(1)].l |= uint32_t(temporary_value_.b << 8);

			temporary_address_.l += 2;
			Access(temporary_value_.low);	// nr
			registers_[instruction_.reg(1)].l |= uint32_t(temporary_value_.b);

			Prefetch();						// np
		MoveToStateSpecific(Decode);

		BeginState(MOVEPtoR_w):
			temporary_address_.l = registers_[8 + instruction_.reg(0)].l + uint32_t(int16_t(prefetch_.w));
			SetDataAddress(temporary_address_.l);
			SetupDataAccess(Microcycle::Read, Microcycle::SelectByte);

			Prefetch();						// np

			Access(temporary_value_.low);	// nR
			registers_[instruction_.reg(1)].w = uint16_t(temporary_value_.b << 8);

			temporary_address_.l += 2;
			Access(temporary_value_.low);	// nr
			registers_[instruction_.reg(1)].w |= uint16_t(temporary_value_.b);

			Prefetch();						// np
		MoveToStateSpecific(Decode);

		//
		// [EORI/ORI/ANDI] #, [CCR/SR]
		//
		BeginState(LogicalToSR):
			IdleBus(4);

			// Perform the operation.
			PerformDynamic();

			// Recede the program counter and prefetch twice.
			program_counter_.l -= 2;
			Prefetch();
			Prefetch();
		MoveToStateSpecific(Decode);

		//
		// MOVEM M --> R
		//
		BeginState(MOVEMtoR):
			post_ea_state_ =
				(instruction_.operation == InstructionSet::M68k::Operation::MOVEMtoRl) ?
					MOVEMtoR_l_read : MOVEMtoR_w_read;
			next_operand_ = 1;
			register_index_ = 0;

			SetDataAddress(effective_address_[1].l);
			SetupDataAccess(Microcycle::Read, Microcycle::SelectWord);
		MoveToStateSpecific(CalcEffectiveAddress);

		BeginState(MOVEMtoR_w_read):
			// If there's nothing left to read, move on.
			if(!operand_[0].w) {
				MoveToStateSpecific(MOVEMtoR_finish);
			}

			// Find the next register to read, read it and sign extend it.
			while(!(operand_[0].w & 1)) {
				operand_[0].w >>= 1;
				++register_index_;
			}
			Access(registers_[register_index_].low);
			registers_[register_index_].l = uint32_t(int16_t(registers_[register_index_].w));
			effective_address_[1].l += 2;

			// Drop the bottom bit.
			operand_[0].w >>= 1;
			++register_index_;
		MoveToStateSpecific(MOVEMtoR_w_read);

		BeginState(MOVEMtoR_l_read):
			// If there's nothing left to read, move on.
			if(!operand_[0].w) {
				MoveToStateSpecific(MOVEMtoR_finish);
			}

			// Find the next register to read, read it.
			while(!(operand_[0].w & 1)) {
				operand_[0].w >>= 1;
				++register_index_;
			}
			Access(registers_[register_index_].high);
			effective_address_[1].l += 2;
			Access(registers_[register_index_].low);
			effective_address_[1].l += 2;

			// Drop the bottom bit.
			operand_[0].w >>= 1;
			++register_index_;
		MoveToStateSpecific(MOVEMtoR_l_read);

		BeginState(MOVEMtoR_finish):
			// Perform one more read, spuriously.
			Access(temporary_value_.low);	// nr

			// Write the address back to the register if
			// this was postincrement mode.
			if(instruction_.mode(1) == Mode::AddressRegisterIndirectWithPostincrement) {
				registers_[8 + instruction_.reg(1)].l = effective_address_[1].l;
			}

			Prefetch();	// np
		MoveToStateSpecific(Decode);

		//
		// MOVEM R --> M
		//
		BeginState(MOVEMtoM):
			next_operand_ = 1;
			SetDataAddress(effective_address_[1].l);
			SetupDataAccess(0, Microcycle::SelectWord);

			// Predecrement writes registers the other way around, but still reads the
			// mask from LSB.
			if(instruction_.mode(1) == Mode::AddressRegisterIndirectWithPredecrement) {
				register_index_ = 15;
				effective_address_[1].l = registers_[8 + instruction_.reg(1)].l;

				// Don't go through the usual calculate EA path because: (i) the test above
				// has already told us the addressing mode, and it's trivial; and (ii) the
				// predecrement isn't actually wanted.
				if(instruction_.operation == InstructionSet::M68k::Operation::MOVEMtoMl) {
					MoveToStateSpecific(MOVEMtoM_l_write_predec);
				} else {
					MoveToStateSpecific(MOVEMtoM_w_write_predec);
				}
			}

			register_index_ = 0;
			post_ea_state_ =
				(instruction_.operation == InstructionSet::M68k::Operation::MOVEMtoMl) ?
					MOVEMtoM_l_write : MOVEMtoM_w_write;
		MoveToStateSpecific(CalcEffectiveAddress);

		BeginState(MOVEMtoM_w_write):
			// If there's nothing left to read, move on.
			if(!operand_[0].w) {
				MoveToStateSpecific(MOVEMtoM_finish);
			}

			// Find the next register to write, write it.
			while(!(operand_[0].w & 1)) {
				operand_[0].w >>= 1;
				++register_index_;
			}
			Access(registers_[register_index_].low);
			effective_address_[1].l += 2;

			// Drop the bottom bit.
			operand_[0].w >>= 1;
			++register_index_;
		MoveToStateSpecific(MOVEMtoM_w_write);

		BeginState(MOVEMtoM_l_write):
			// If there's nothing left to read, move on.
			if(!operand_[0].w) {
				MoveToStateSpecific(MOVEMtoM_finish);
			}

			// Find the next register to write, write it.
			while(!(operand_[0].w & 1)) {
				operand_[0].w >>= 1;
				++register_index_;
			}

			Access(registers_[register_index_].high);
			effective_address_[1].l += 2;
			Access(registers_[register_index_].low);
			effective_address_[1].l += 2;

			// Drop the bottom bit.
			operand_[0].w >>= 1;
			++register_index_;
		MoveToStateSpecific(MOVEMtoM_l_write);

		BeginState(MOVEMtoM_w_write_predec):
			// If there's nothing left to read, move on.
			if(!operand_[0].w) {
				MoveToStateSpecific(MOVEMtoM_finish);
			}

			// Find the next register to write, write it.
			while(!(operand_[0].w & 1)) {
				operand_[0].w >>= 1;
				--register_index_;
			}
			effective_address_[1].l -= 2;
			Access(registers_[register_index_].low);

			// Drop the bottom bit.
			operand_[0].w >>= 1;
			--register_index_;
		MoveToStateSpecific(MOVEMtoM_w_write_predec);

		BeginState(MOVEMtoM_l_write_predec):
			// If there's nothing left to read, move on.
			if(!operand_[0].w) {
				MoveToStateSpecific(MOVEMtoM_finish);
			}

			// Find the next register to write, write it.
			while(!(operand_[0].w & 1)) {
				operand_[0].w >>= 1;
				--register_index_;
			}

			effective_address_[1].l -= 2;
			Access(registers_[register_index_].low);
			effective_address_[1].l -= 2;
			Access(registers_[register_index_].high);

			// Drop the bottom bit.
			operand_[0].w >>= 1;
			--register_index_;
		MoveToStateSpecific(MOVEMtoM_l_write_predec);

		BeginState(MOVEMtoM_finish):
			// Write the address back to the register if
			// this was predecrement mode.
			if(instruction_.mode(1) == Mode::AddressRegisterIndirectWithPredecrement) {
				registers_[8 + instruction_.reg(1)].l = effective_address_[1].l;
			}

			Prefetch();	// np
		MoveToStateSpecific(Decode);

		//
		// DIVU and DIVUS
		//
		BeginState(DIVU_DIVS):
			// Set a no-interrupt-occurred sentinel.
			exception_vector_ = -1;

			// Perform the instruction.
			PerformDynamic();

			// Delay the correct amount of time.
			IdleBus(dynamic_instruction_length_);

			// Either dispatch an exception or don't.
			if(exception_vector_ >= 0) {
				MoveToStateSpecific(StandardException);
			}

			// DIVU and DIVS are always to a register, so just write back here
			// to save on dispatch costs.
			registers_[instruction_.reg(1)] = operand_[1];

			Prefetch();		// np
		MoveToStateSpecific(Decode);

		//
		// MULU, MULS and shifts
		//
		BeginState(Perform_idle_dyamic_Dn):
			Prefetch();		// np

			// Perform the instruction.
			PerformDynamic();

			// Delay the correct amount of time.
			IdleBus(dynamic_instruction_length_);

			// MULU and MULS are always to a register, so just write back here
			// to save on dispatch costs.
			registers_[instruction_.reg(1)] = operand_[1];

		MoveToStateSpecific(Decode);

		//
		// LEA
		//
		BeginState(LEA):
			registers_[8 + instruction_.reg(1)].l = effective_address_[0].l;
			Prefetch();
		MoveToStateSpecific(Decode);

		//
		// PEA
		//
		BeginState(PEA):
			Push(effective_address_[0]);
			Prefetch();
		MoveToStateSpecific(Decode);

		//
		// TAS
		//
		BeginState(TAS):
			// Populate all addresses.
			tas_cycles[0].address = tas_cycles[1].address =
			tas_cycles[2].address =
			tas_cycles[3].address = tas_cycles[4].address = &effective_address_[0].l;

			// Populate values to the relevant subset.
			tas_cycles[0].value = tas_cycles[1].value =
			tas_cycles[3].value = tas_cycles[4].value = &operand_[0].low;

			// First two parts: the read.
			PerformBusOperation(tas_cycles[0]);
			CompleteAccess(tas_cycles[1]);

			// Third part: processing time.
			PerformBusOperation(tas_cycles[2]);

			// Do the actual TAS operation.
			status_.overflow_flag = status_.carry_flag = 0;
			status_.zero_result = operand_[0].b;
			status_.negative_flag = operand_[0].b & 0x80;

			// Final parts: write back.
			operand_[0].b |= 0x80;
			PerformBusOperation(tas_cycles[3]);
			CompleteAccess(tas_cycles[4]);

			Prefetch();
		MoveToStateSpecific(Decode);

		//
		// MOVE to [CCR/SR]
		//
		BeginState(MOVEtoCCRSR):
			PerformDynamic();

			// Rewind the program counter and prefetch twice.
			IdleBus(2);
			program_counter_.l -= 2;
			Prefetch();
			Prefetch();
		MoveToStateSpecific(Decode);

		//
		// RTR, RTS, RTE
		//
		BeginState(RTS):
			SetupDataAccess(Microcycle::Read, Microcycle::SelectWord);
			SetDataAddress(registers_[15].l);

			Access(program_counter_.high);
			registers_[15].l += 2;
			Access(program_counter_.low);
			registers_[15].l += 2;

			Prefetch();
			Prefetch();
		MoveToStateSpecific(Decode);

		BeginState(RTE):
			SetupDataAccess(Microcycle::Read, Microcycle::SelectWord);
			SetDataAddress(registers_[15].l);

			registers_[15].l += 2;
			Access(program_counter_.high);
			registers_[15].l += 2;
			Access(program_counter_.low);

			registers_[15].l -= 4;
			Access(temporary_value_.low);
			registers_[15].l += 6;
			status_.set_status(temporary_value_.w);
			did_update_status();

			Prefetch();
			Prefetch();
		MoveToStateSpecific(Decode);

		BeginState(RTR):
			SetupDataAccess(Microcycle::Read, Microcycle::SelectWord);
			SetDataAddress(registers_[15].l);

			registers_[15].l += 2;
			Access(program_counter_.high);
			registers_[15].l += 2;
			Access(program_counter_.low);

			registers_[15].l -= 4;
			Access(temporary_value_.low);
			registers_[15].l += 6;
			status_.set_ccr(temporary_value_.w);

			Prefetch();
			Prefetch();
		MoveToStateSpecific(Decode);

		//
		// LINK[.w] and UNLINK
		//
		BeginState(LINKw):
			Prefetch();

			// Ensure that the stack pointer is [seemingly] captured after
			// having been decremented by four, if it's what should be captured.
			registers_[15].l -= 4;
			temporary_address_ = registers_[8 + instruction_.reg(0)];
			registers_[15].l += 4;

			// Push will actually decrement the stack pointer.
			Push(temporary_address_);

			// Make the exchange.
			registers_[8 + instruction_.reg(0)].l = registers_[15].l;
			registers_[15].l += uint32_t(int16_t(prefetch_.high.w));

			Prefetch();
		MoveToStateSpecific(Decode);

		BeginState(UNLINK):
			registers_[15] = registers_[8 + instruction_.reg(0)];
			Pop(temporary_address_);
			registers_[8 + instruction_.reg(0)] = temporary_address_;
			Prefetch();
		MoveToStateSpecific(Decode);

		//
		// RESET
		//
		BeginState(RESET):
			IdleBus(2);
			PerformBusOperation(reset_cycle);
			Prefetch();
		MoveToStateSpecific(Decode);

		//
		// NOP
		//
		BeginState(NOP):
			Prefetch();
		MoveToStateSpecific(Decode);

		//
		// TRAP, TRAPV
		//

		// TODO: which program counter is appropriate for TRAP? That of the TRAP,
		// or that of the instruction after?
		BeginState(TRAP):
			IdleBus(2);
			instruction_address_.l += 2;	// Push the address of the instruction after the trap.
		RaiseException((opcode_ & 15) + InstructionSet::M68k::Exception::TrapBase);

		BeginState(TRAPV):
			Prefetch();
			if(!status_.overflow_flag) {
				MoveToStateSpecific(Decode);
			}
			instruction_address_.l += 2;	// Push the address of the instruction after the trap.
		RaiseException(InstructionSet::M68k::Exception::TRAPV);

		default:
			printf("Unhandled state: %d; opcode is %04x\n", state_, opcode_);
			assert(false);
	}}

#undef Pop
#undef Push
#undef PerformDynamic
#undef PerformSpecific
#undef RaiseException
#undef Prefetch
#undef ReadProgramWord
#undef ReadDataWord
#undef AccessPair
#undef CompleteAccess
#undef WaitForDTACK
#undef IdleBus
#undef PerformBusOperation
#undef MoveToStateSpecific
#undef MoveToStateDynamic
#undef CheckOverrun
#undef Spend
#undef ConsiderExit
#undef ReloadInstructionAddress

}

// MARK: - Flow Controller.

void ProcessorBase::did_update_status() {
	// Shuffle the stack pointers.
	stack_pointers_[is_supervisor_] = registers_[15];
	registers_[15] = stack_pointers_[int(status_.is_supervisor)];
	is_supervisor_ = int(status_.is_supervisor);
}

void ProcessorBase::did_chk(bool was_under, bool was_over) {
	if(was_over) {
		state_ = CHK_was_over;
	} else if(was_under) {
		state_ = CHK_was_under;
	} else {
		state_ = CHK_no_trap;
	}
}

void ProcessorBase::did_scc(bool did_set_ff) {
	state_ = did_set_ff ? Scc_Dn_did_set : Scc_Dn_did_not_set;
}

void ProcessorBase::complete_dbcc(bool matched_condition, bool overflowed, int16_t offset) {
	// The actual DBcc rule is: branch if !matched_condition and !overflowed; but I think
	// that a spurious read from the intended destination PC occurs if overflowed, so update
	// the PC for any case of !matched_condition and rely on the DBcc_counter_overflow to
	// set it back.
	if(!matched_condition) {
		state_ = overflowed ? DBcc_counter_overflow : DBcc_branch_taken;
		program_counter_.l = instruction_address_.l + uint32_t(offset) + 2;
		return;
	}
	state_ = DBcc_condition_true;
}

template <typename IntT> void ProcessorBase::complete_bcc(bool take_branch, IntT offset) {
	if(take_branch) {
		program_counter_.l = instruction_address_.l + uint32_t(offset) + 2;
		state_ = Bcc_branch_taken;
		return;
	}

	state_ =
		(instruction_.operation == InstructionSet::M68k::Operation::Bccb) ?
			Bccb_branch_not_taken : Bccw_branch_not_taken;
}

void ProcessorBase::did_bit_op(int bit_position) {
	dynamic_instruction_length_ = int(bit_position > 15);
}

template <bool did_overflow> void ProcessorBase::did_divu(uint32_t dividend, uint32_t divisor) {
	if(!divisor) {
		dynamic_instruction_length_ = 4;	// nn nn precedes the usual exception activity.
		return;
	}

	if(did_overflow) {
		dynamic_instruction_length_ = 3;	// Covers the nn n to get into the loop.
		return;
	}

	// Calculate cost; this is based on the flowchart in yacht.txt.
	// I could actually calculate the division result using this code,
	// since this is a classic divide algorithm, but would rather that
	// errors produce incorrect timing only, not incorrect timing plus
	// incorrect results.
	dynamic_instruction_length_ =
		3 +		// nn n to get into the loop;
		30 +	// nn per iteration of the loop below;
		3;		// n nn upon completion of the loop.

	divisor <<= 16;
	for(int c = 0; c < 15; ++c) {
		if(dividend & 0x8000'0000) {
			dividend = (dividend << 1) - divisor;
		} else {
			dividend <<= 1;

			// Yacht.txt, and indeed a real microprogram, would just subtract here
			// and test the sign of the result, but this is easier to follow:
			if (dividend >= divisor) {
				dividend -= divisor;
				dynamic_instruction_length_ += 1;	// i.e. the original nn plus one further n before going down the MSB=0 route.
			} else {
				dynamic_instruction_length_ += 2;	// The costliest path (since in real life it's a subtraction and then a step
				// back from there) — all costs accrue. So the fixed nn loop plus another n,
				// plus another one.
			}
		}
	}
}

#define convert_to_bit_count_16(x)			\
	x = ((x & 0xaaaa) >> 1) + (x & 0x5555);	\
	x = ((x & 0xcccc) >> 2) + (x & 0x3333);	\
	x = ((x & 0xf0f0) >> 4) + (x & 0x0f0f);	\
	x = ((x & 0xff00) >> 8) + (x & 0x00ff);

template <bool did_overflow> void ProcessorBase::did_divs(int32_t dividend, int32_t divisor) {
	// The route to spotting divide by 0 is just nn nn.
	if(!divisor) {
		dynamic_instruction_length_ = 4;	// nn nn precedes the usual exception activity.
		return;
	}

	// It's either six or seven microcycles to get into the main loop, depending
	// on dividend sign.
	dynamic_instruction_length_ = 6 + (dividend < 0);

	if(did_overflow) {
		return;
	}

	// There's a fixed cost per bit, plus an additional one for each that is zero.
	//
	// The sign bit does not count here; it's the high fifteen bits that matter
	// only, in the unsigned version of the result.
	//
	// Disclaimer: per the flowchart it looks to me like this constant should be 60
	// rather than 49 — four microcycles per bit. But the number 49 makes this
	// algorithm exactly fit the stated minimum and maximum costs. Possibly the
	// undefined difference between a nop cycle an an idle wait is relevant here?
	dynamic_instruction_length_ += 49;

	int result_bits = ~abs(dividend / divisor) & 0xfffe;
	convert_to_bit_count_16(result_bits);
	dynamic_instruction_length_ += result_bits;

	// Determine the tail cost; a divisor of less than 0 leads to one exit,
	// a divisor of greater than zero makes the result a function of the
	// sign of the dividend.
	//
	// In all cases, this is counting from 'No more bits' in the Yacht diagram.
	if(divisor < 0) {
		dynamic_instruction_length_ += 4;
		return;
	}

	if(dividend < 0) {
		dynamic_instruction_length_ += 5;
	} else {
		dynamic_instruction_length_ += 3;
	}
}

template <typename IntT> void ProcessorBase::did_mulu(IntT multiplier) {
	// Count number of bits set.
	convert_to_bit_count_16(multiplier);
	dynamic_instruction_length_ = 17 + multiplier;
}

template <typename IntT> void ProcessorBase::did_muls(IntT multiplier) {
	// Count number of transitions from 0 to 1 or from 1 to 0 — i.e. the
	// number of times that a bit is not equal to the one to its right.
	// Treat the bit to the right of b0 as 0.
	int number_of_pairs = (multiplier ^ (multiplier << 1)) & 0xffff;
	convert_to_bit_count_16(number_of_pairs);
	dynamic_instruction_length_ = 17 + number_of_pairs;
}

#undef convert_to_bit_count_16

template <typename IntT> void ProcessorBase::did_shift(int bits_shifted) {
	if constexpr (sizeof(IntT) == 4) {
		dynamic_instruction_length_ = bits_shifted + 2;
	} else {
		dynamic_instruction_length_ = bits_shifted + 1;
	}
}

template <bool use_current_instruction_pc> void ProcessorBase::raise_exception(int vector) {
	// No overt action is taken here; instructions that might throw an exception are required
	// to check-in after the fact.
	//
	// As implemented above, that means:
	//
	//	* DIVU;
	//	* DIVS.
	exception_vector_ = vector;
}

inline void ProcessorBase::tas(Preinstruction instruction, uint32_t) {
	// This will be reached only if addressing mode is Dn.
	const uint8_t value = registers_[instruction.reg(0)].b;
	registers_[instruction.reg(0)].b |= 0x80;

	status_.overflow_flag = status_.carry_flag = 0;
	status_.zero_result = value;
	status_.negative_flag = value & 0x80;
}

inline void ProcessorBase::move_to_usp(uint32_t address) {
	stack_pointers_[0].l = address;
}

inline void ProcessorBase::move_from_usp(uint32_t &address) {
	address = stack_pointers_[0].l;
}

// MARK: - External state.

template <class BusHandler, bool dtack_is_implicit, bool permit_overrun, bool signal_will_perform>
CPU::MC68000Mk2::State Processor<BusHandler, dtack_is_implicit, permit_overrun, signal_will_perform>::get_state() {
	CPU::MC68000Mk2::State state;

	// This isn't true, but will ensure that both stack_pointers_ have their proper values.
	did_update_status();

	for(int c = 0; c < 7; c++) {
		state.registers.data[c] = registers_[c].l;
		state.registers.address[c] = registers_[c + 8].l;
	}
	state.registers.data[7] = registers_[7].l;

	state.registers.program_counter = program_counter_.l;
	state.registers.status = status_.status();
	state.registers.user_stack_pointer = stack_pointers_[0].l;
	state.registers.supervisor_stack_pointer = stack_pointers_[1].l;

	return state;
}

template <class BusHandler, bool dtack_is_implicit, bool permit_overrun, bool signal_will_perform>
void Processor<BusHandler, dtack_is_implicit, permit_overrun, signal_will_perform>::set_state(const CPU::MC68000Mk2::State &state) {
	// Copy registers and the program counter.
	for(int c = 0; c < 7; c++) {
		registers_[c].l = state.registers.data[c];
		registers_[c + 8].l = state.registers.address[c];
	}
	registers_[7].l = state.registers.data[7];
	program_counter_.l = state.registers.program_counter;

	// Set status first in order to get the proper is-supervisor flag in place.
	status_.set_status(state.registers.status);

	// Update stack pointers, being careful to copy the right one.
	stack_pointers_[0].l = state.registers.user_stack_pointer;
	stack_pointers_[1].l = state.registers.supervisor_stack_pointer;
	registers_[15] = stack_pointers_[is_supervisor_];

	// Ensure the local is-supervisor flag is updated.
	did_update_status();
}

template <class BusHandler, bool dtack_is_implicit, bool permit_overrun, bool signal_will_perform>
void Processor<BusHandler, dtack_is_implicit, permit_overrun, signal_will_perform>::decode_from_state(const InstructionSet::M68k::RegisterSet &registers) {
	// Populate registers.
	CPU::MC68000Mk2::State state;
	state.registers = registers;
	set_state(state);

	// Ensure the state machine will resume at decode.
	state_ = Decode;

	// Fill the prefetch queue.
	captured_interrupt_level_ = bus_interrupt_level_;

	read_program.value = &prefetch_.high;
	bus_handler_.perform_bus_operation(read_program_announce, is_supervisor_);
	bus_handler_.perform_bus_operation(read_program, is_supervisor_);
	program_counter_.l += 2;

	read_program.value = &prefetch_.low;
	bus_handler_.perform_bus_operation(read_program_announce, is_supervisor_);
	bus_handler_.perform_bus_operation(read_program, is_supervisor_);
	program_counter_.l += 2;
}

template <class BusHandler, bool dtack_is_implicit, bool permit_overrun, bool signal_will_perform>
void Processor<BusHandler, dtack_is_implicit, permit_overrun, signal_will_perform>::reset() {
	state_ = Reset;
	time_remaining_ = HalfCycles(0);
}

}
}

#endif /* _8000Mk2Implementation_h */
