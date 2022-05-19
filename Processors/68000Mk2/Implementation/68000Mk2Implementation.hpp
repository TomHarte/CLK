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
	StoreOperand_l,

	// Specific addressing mode fetches.

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

	// Various forms of perform; each of these will
	// perform the current instruction, then do the
	// indicated bus cycle.

	Perform_np,
	Perform_np_n,
	Perform_np_nn,

	// MOVE has unique bus usage, so has specialised states.

	MOVEw,
	MOVEwRegisterDirect,
	MOVEwAddressRegisterIndirectWithPostincrement,
};

// MARK: - The state machine.

template <class BusHandler, bool dtack_is_implicit, bool permit_overrun, bool signal_will_perform>
void Processor<BusHandler, dtack_is_implicit, permit_overrun, signal_will_perform>::run_for(HalfCycles duration) {
	// Accumulate the newly paid-in cycles. If this instance remains in deficit, exit.
	time_remaining_ += duration;
	if(time_remaining_ <= HalfCycles(0)) return;

	// Check whether all remaining time has been expended; if so then exit, having set this line up as
	// the next resumption point.
#define ConsiderExit()	if(time_remaining_ <= HalfCycles(0)) { state_ = __COUNTER__+1; return; } [[fallthrough]]; case __COUNTER__:

	// Subtracts `n` half-cycles from `time_remaining_`; if permit_overrun is false, also ConsiderExit()
#define Spend(n)		time_remaining_ -= (n); if constexpr (!permit_overrun) ConsiderExit()

	// Performs ConsiderExit() only if permit_overrun is true.
#define CheckOverrun()	if constexpr (permit_overrun) ConsiderExit()

	// Sets `x` as the next state, and exits now if all remaining time has been extended and permit_overrun is true.
	// Jumps directly to the state otherwise.
#define MoveToState(x)	{ state_ = ExecutionState::x; goto x; }

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

	// Performs the bus operation and then applies a `Spend` of its length
	// plus any additional length returned by the bus handler.
#define PerformBusOperation(x)										\
	delay = bus_handler_.perform_bus_operation(x, is_supervisor_);	\
	Spend(x.length + delay)

	// Performs no bus activity for the specified number of microcycles.
#define IdleBus(n)						\
	idle.length = HalfCycles(n * 4);	\
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
	// bus if VPA is currently asserted.
	//
	// TODO: If BERR is asserted, stop here and perform a bus error exception.
	//
	// TODO: If VPA is asserted, stretch this cycle.
#define CompleteAccess(x)												\
	PerformBusOperation(x)

	// Performs the memory access implied by the announce, perform pair,
	// honouring DTACK, BERR and VPA as necessary.
#define AccessPair(val, announce, perform)			\
	perform.value = &val;							\
	if constexpr (!dtack_is_implicit) {				\
		announce.length = HalfCycles(4);			\
	}												\
	PerformBusOperation(announce);					\
	WaitForDTACK(announce);							\
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
#define Prefetch()					\
	prefetch_.high = prefetch_.low;	\
	ReadProgramWord(prefetch_.low)

	using Mode = InstructionSet::M68k::AddressingMode;

	// Otherwise continue for all time, until back in debt.
	// Formatting is slightly obtuse here to make this look more like a coroutine.
	while(true) { switch(state_) {

		// Spin in place, one cycle at a time, until one of DTACK,
		// BERR or VPA is asserted.
		BeginState(WaitForDTACK):
			PerformBusOperation(awaiting_dtack);

			if(dtack_ || berr_ || vpa_) {
				state_ = post_dtack_state_;
				continue;
			}
		MoveToState(WaitForDTACK);

		// Perform the RESET exception, which seeds the stack pointer and program
		// counter, populates the prefetch queue, and then moves to instruction dispatch.
		BeginState(Reset):
			IdleBus(7);			// (n-)*5   nn

			// Establish general reset state.
			status_.is_supervisor = true;
			status_.interrupt_level = 7;
			status_.trace_flag = 0;
			did_update_status();

			SetupDataAccess(Microcycle::Read, Microcycle::SelectWord);
			SetDataAddress(temporary_address_);

			temporary_address_ = 0;
			Access(registers_[15].high);	// nF

			temporary_address_ += 2;
			Access(registers_[15].low);		// nf

			temporary_address_ += 2;
			Access(program_counter_.high);	// nV

			temporary_address_ += 2;
			Access(program_counter_.low);	// nv

			Prefetch();			// np
			IdleBus(1);			// n
			Prefetch();			// np
		MoveToState(Decode);

		// Inspect the prefetch queue in order to decode the next instruction,
		// and segue into the fetching of operands.
		BeginState(Decode):
			CheckOverrun();

			opcode_ = prefetch_.high.w;
			instruction_ = decoder_.decode(opcode_);
			instruction_address_ = program_counter_.l - 4;

			// TODO: check for privilege and unrecognised instructions.

			// Signal the bus handler if requested.
			if constexpr (signal_will_perform) {
				bus_handler_.will_perform(instruction_address_, opcode_);
			}

			// Ensure the first parameter is next fetched.
			next_operand_ = 0;

			// Obtain operand flags and pick a perform pattern.
#define CASE(x)	\
	case InstructionSet::M68k::Operation::x:																								\
		operand_flags_ = InstructionSet::M68k::operand_flags<InstructionSet::M68k::Model::M68000, InstructionSet::M68k::Operation::x>();

#define StdCASE(x, y)	\
	CASE(x)	\
		y;	\
		\
		if constexpr (InstructionSet::M68k::operand_size<InstructionSet::M68k::Operation::x>() == InstructionSet::M68k::DataSize::LongWord) {	\
			SetupDataAccess(Microcycle::Read, Microcycle::SelectWord);	\
			MoveToState(FetchOperand_l);	\
		} else {	\
			if constexpr (InstructionSet::M68k::operand_size<InstructionSet::M68k::Operation::x>() == InstructionSet::M68k::DataSize::Byte) {	\
				SetupDataAccess(Microcycle::Read, Microcycle::SelectByte);	\
			} else {	\
				SetupDataAccess(Microcycle::Read, Microcycle::SelectWord);	\
			}	\
			MoveToState(FetchOperand_bw);	\
		}

			switch(instruction_.operation) {
				StdCASE(NBCD, {
					if(instruction_.mode(0) == Mode::DataRegisterDirect) {
						perform_state_ = Perform_np_n;
					} else {
						perform_state_ = Perform_np;
					}
				})

				StdCASE(SWAP, 		perform_state_ = Perform_np);

				StdCASE(MOVEw,		perform_state_ = MOVEw);

				StdCASE(CMPb,		perform_state_ = Perform_np);
				StdCASE(CMPw,		perform_state_ = Perform_np);
				StdCASE(CMPl,		perform_state_ = Perform_np_n);

				StdCASE(CMPAw,		perform_state_ = Perform_np_n);
				StdCASE(CMPAl,		perform_state_ = Perform_np_n);

				StdCASE(ANDb,		perform_state_ = Perform_np);
				StdCASE(ANDw,		perform_state_ = Perform_np);
				StdCASE(ANDl, {
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

				StdCASE(ORb,		perform_state_ = Perform_np);
				StdCASE(ORw,		perform_state_ = Perform_np);
				StdCASE(ORl, {
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

				default:
					assert(false);
			}

#undef CASE

	// MARK: - Fetch, dispatch.

#define MoveToNextOperand(x)		\
	++next_operand_;				\
	if(next_operand_ == 2) {		\
		state_ = perform_state_;	\
		continue;					\
	}								\
	MoveToState(x)

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
				state_ = perform_state_;
				continue;
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
					MoveToState(FetchAddressRegisterIndirect_bw);
				case Mode::AddressRegisterIndirectWithPostincrement:
					MoveToState(FetchAddressRegisterIndirectWithPostincrement_bw);
				case Mode::AddressRegisterIndirectWithPredecrement:
					MoveToState(FetchAddressRegisterIndirectWithPredecrement_bw);
				case Mode::AddressRegisterIndirectWithDisplacement:
					MoveToState(FetchAddressRegisterIndirectWithDisplacement_bw);
				case Mode::AddressRegisterIndirectWithIndex8bitDisplacement:
					MoveToState(FetchAddressRegisterIndirectWithIndex8bitDisplacement_bw);
				case Mode::ProgramCounterIndirectWithDisplacement:
					MoveToState(FetchProgramCounterIndirectWithDisplacement_bw);
				case Mode::ProgramCounterIndirectWithIndex8bitDisplacement:
					MoveToState(FetchProgramCounterIndirectWithIndex8bitDisplacement_bw);
				case Mode::AbsoluteShort:
					MoveToState(FetchAbsoluteShort_bw);
				case Mode::AbsoluteLong:
					MoveToState(FetchAbsoluteLong_bw);
				case Mode::ImmediateData:
					MoveToState(FetchImmediateData_bw);

				// Should be impossible to reach.
				default:
					assert(false);
			}
		break;

		// As above, but for .l.
		BeginState(FetchOperand_l):
			if(!(operand_flags_ & (1 << next_operand_))) {
				state_ = perform_state_;
				continue;
			}

			switch(instruction_.mode(next_operand_)) {
				case Mode::AddressRegisterDirect:
				case Mode::DataRegisterDirect:
					operand_[next_operand_] = registers_[instruction_.lreg(next_operand_)];
				MoveToNextOperand(FetchOperand_bw);

				case Mode::Quick:
					operand_[next_operand_].l = InstructionSet::M68k::quick(opcode_, instruction_.operation);
				MoveToNextOperand(FetchOperand_bw);

				case Mode::AddressRegisterIndirect:
					MoveToState(FetchAddressRegisterIndirect_l);
				case Mode::AddressRegisterIndirectWithPostincrement:
					MoveToState(FetchAddressRegisterIndirectWithPostincrement_l);
				case Mode::AddressRegisterIndirectWithPredecrement:
					MoveToState(FetchAddressRegisterIndirectWithPredecrement_l);
				case Mode::AddressRegisterIndirectWithDisplacement:
					MoveToState(FetchAddressRegisterIndirectWithDisplacement_l);
				case Mode::AddressRegisterIndirectWithIndex8bitDisplacement:
					MoveToState(FetchAddressRegisterIndirectWithIndex8bitDisplacement_l);
				case Mode::ProgramCounterIndirectWithDisplacement:
					MoveToState(FetchProgramCounterIndirectWithDisplacement_l);
				case Mode::ProgramCounterIndirectWithIndex8bitDisplacement:
					MoveToState(FetchProgramCounterIndirectWithIndex8bitDisplacement_l);
				case Mode::AbsoluteShort:
					MoveToState(FetchAbsoluteShort_l);
				case Mode::AbsoluteLong:
					MoveToState(FetchAbsoluteLong_l);
				case Mode::ImmediateData:
					MoveToState(FetchImmediateData_l);

				// Should be impossible to reach.
				default:
					assert(false);
			}
		break;

	// MARK: - Fetch, addressing modes.

		//
		// AddressRegisterIndirect
		//
		BeginState(FetchAddressRegisterIndirect_bw):
			effective_address_[next_operand_] = registers_[8 + instruction_.reg(next_operand_)].l;
			SetDataAddress(effective_address_[next_operand_]);

			Access(operand_[next_operand_].low);	// nr
		MoveToNextOperand(FetchOperand_bw);

		BeginState(FetchAddressRegisterIndirect_l):
			effective_address_[next_operand_] = registers_[8 + instruction_.reg(next_operand_)].l;
			SetDataAddress(effective_address_[next_operand_]);

			Access(operand_[next_operand_].high);	// nR

			effective_address_[next_operand_] += 2;
			Access(operand_[next_operand_].low);	// nr
		MoveToNextOperand(FetchOperand_l);

		//
		// AddressRegisterIndirectWithPostincrement
		//
		BeginState(FetchAddressRegisterIndirectWithPostincrement_bw):
			effective_address_[next_operand_] = registers_[8 + instruction_.reg(next_operand_)].l;
			registers_[8 + instruction_.reg(next_operand_)].l +=
				byte_word_increments[int(instruction_.operand_size())][instruction_.reg(next_operand_)];

			SetDataAddress(effective_address_[next_operand_]);
			Access(operand_[next_operand_].low);	// nr
		MoveToNextOperand(FetchOperand_bw);

		BeginState(FetchAddressRegisterIndirectWithPostincrement_l):
			effective_address_[next_operand_] = registers_[8 + instruction_.reg(next_operand_)].l;
			registers_[8 + instruction_.reg(next_operand_)].l += 4;

			SetDataAddress(effective_address_[next_operand_]);
			Access(operand_[next_operand_].high);	// nR
			effective_address_[next_operand_] += 2;
			Access(operand_[next_operand_].low);	// nr
		MoveToNextOperand(FetchOperand_l);

		//
		// AddressRegisterIndirectWithPredecrement
		//
		BeginState(FetchAddressRegisterIndirectWithPredecrement_bw):
			registers_[8 + instruction_.reg(next_operand_)].l -=
				byte_word_increments[int(instruction_.operand_size())][instruction_.reg(next_operand_)];
			effective_address_[next_operand_] = registers_[8 + instruction_.reg(next_operand_)].l;
			SetDataAddress(effective_address_[next_operand_]);

			IdleBus(1);								// n
			Access(operand_[next_operand_].low);	// nr
		MoveToNextOperand(FetchOperand_bw);

		BeginState(FetchAddressRegisterIndirectWithPredecrement_l):
			registers_[8 + instruction_.reg(next_operand_)].l -= 4;
			effective_address_[next_operand_] = registers_[8 + instruction_.reg(next_operand_)].l;
			SetDataAddress(effective_address_[next_operand_]);

			IdleBus(1);								// n
			Access(operand_[next_operand_].high);	// nR
			effective_address_[next_operand_] += 2;
			Access(operand_[next_operand_].low);	// nr
		MoveToNextOperand(FetchOperand_l);

		//
		// AddressRegisterIndirectWithDisplacement
		//
		BeginState(FetchAddressRegisterIndirectWithDisplacement_bw):
			effective_address_[next_operand_] =
				registers_[8 + instruction_.reg(next_operand_)].l +
				int16_t(prefetch_.w);
			SetDataAddress(effective_address_[next_operand_]);

			Prefetch();								// np
			Access(operand_[next_operand_].low);	// nr
		MoveToNextOperand(FetchOperand_bw);

		BeginState(FetchAddressRegisterIndirectWithDisplacement_l):
			effective_address_[next_operand_] =
				registers_[8 + instruction_.reg(next_operand_)].l +
				int16_t(prefetch_.w);
			SetDataAddress(effective_address_[next_operand_]);

			Prefetch();								// np
			Access(operand_[next_operand_].high);	// nR
			effective_address_[next_operand_] += 2;
			Access(operand_[next_operand_].low);	// nr
		MoveToNextOperand(FetchOperand_l);

		//
		// ProgramCounterIndirectWithDisplacement
		//
		BeginState(FetchProgramCounterIndirectWithDisplacement_bw):
			effective_address_[next_operand_] =
				instruction_address_ + 2 +
				int16_t(prefetch_.w);
			SetDataAddress(effective_address_[next_operand_]);

			Prefetch();								// np
			Access(operand_[next_operand_].low);	// nr
		MoveToNextOperand(FetchOperand_bw);

		BeginState(FetchProgramCounterIndirectWithDisplacement_l):
			effective_address_[next_operand_] =
				instruction_address_ + 2 +
				int16_t(prefetch_.w);
			SetDataAddress(effective_address_[next_operand_]);

			Prefetch();								// np
			Access(operand_[next_operand_].high);	// nR
			effective_address_[next_operand_] += 2;
			Access(operand_[next_operand_].low);	// nr
		MoveToNextOperand(FetchOperand_l);

#define d8Xn(base)										\
	base +												\
	((prefetch_.w & 0x800) ?							\
		registers_[prefetch_.w >> 12].l :				\
		int16_t(registers_[prefetch_.w >> 12].w)) +		\
	int8_t(prefetch_.b);

		//
		// AddressRegisterIndirectWithIndex8bitDisplacement
		//
		BeginState(FetchAddressRegisterIndirectWithIndex8bitDisplacement_bw):
			effective_address_[next_operand_] = d8Xn(registers_[8 + instruction_.reg(next_operand_)].l);
			SetDataAddress(effective_address_[next_operand_]);

			IdleBus(1);								// n
			Prefetch();								// np
			Access(operand_[next_operand_].low);	// nr
		MoveToNextOperand(FetchOperand_bw);

		BeginState(FetchAddressRegisterIndirectWithIndex8bitDisplacement_l):
			effective_address_[next_operand_] = d8Xn(registers_[8 + instruction_.reg(next_operand_)].l);
			SetDataAddress(effective_address_[next_operand_]);

			IdleBus(1);								// n
			Prefetch();								// np
			Access(operand_[next_operand_].high);	// nR
			effective_address_[next_operand_] += 2;
			Access(operand_[next_operand_].low);	// nr
		MoveToNextOperand(FetchOperand_l);

		//
		// ProgramCounterIndirectWithIndex8bitDisplacement
		//
		BeginState(FetchProgramCounterIndirectWithIndex8bitDisplacement_bw):
			effective_address_[next_operand_] = d8Xn(instruction_address_ + 2);
			SetDataAddress(effective_address_[next_operand_]);

			IdleBus(1);								// n
			Prefetch();								// np
			Access(operand_[next_operand_].low);	// nr
		MoveToNextOperand(FetchOperand_bw);

		BeginState(FetchProgramCounterIndirectWithIndex8bitDisplacement_l):
			effective_address_[next_operand_] = d8Xn(instruction_address_ + 2);;
			SetDataAddress(effective_address_[next_operand_]);

			IdleBus(1);								// n
			Prefetch();								// np
			Access(operand_[next_operand_].high);	// nR
			effective_address_[next_operand_] += 2;
			Access(operand_[next_operand_].low);	// nr
		MoveToNextOperand(FetchOperand_l);

#undef d8Xn

		//
		// AbsoluteShort
		//
		BeginState(FetchAbsoluteShort_bw):
			effective_address_[next_operand_] = int16_t(prefetch_.w);
			SetDataAddress(effective_address_[next_operand_]);

			Prefetch();								// np
			Access(operand_[next_operand_].low);	// nr
		MoveToNextOperand(FetchOperand_bw);

		BeginState(FetchAbsoluteShort_l):
			effective_address_[next_operand_] = int16_t(prefetch_.w);
			SetDataAddress(effective_address_[next_operand_]);

			Prefetch();								// np
			Access(operand_[next_operand_].high);	// nR
			effective_address_[next_operand_] += 2;
			Access(operand_[next_operand_].low);	// nr
		MoveToNextOperand(FetchOperand_l);

		//
		// AbsoluteLong
		//
		BeginState(FetchAbsoluteLong_bw):
			Prefetch();								// np

			effective_address_[next_operand_] = prefetch_.l;
			SetDataAddress(effective_address_[next_operand_]);

			Prefetch();								// np
			Access(operand_[next_operand_].low);	// nr
		MoveToNextOperand(FetchOperand_bw);

		BeginState(FetchAbsoluteLong_l):
			Prefetch();								// np

			effective_address_[next_operand_] = prefetch_.l;
			SetDataAddress(effective_address_[next_operand_]);

			Prefetch();								// np
			Access(operand_[next_operand_].high);	// nR
			effective_address_[next_operand_] += 2;
			Access(operand_[next_operand_].low);	// nr
		MoveToNextOperand(FetchOperand_l);

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

	// MARK: - Store.

		// Store operand is a lot simpler: only one operand is ever stored, and its address
		// is already known. So this can either skip straight back to ::Decode if the target
		// is a register, otherwise a single write operation can occur.
		BeginState(StoreOperand):
			if(instruction_.mode(next_operand_) <= Mode::AddressRegisterDirect) {
				registers_[instruction_.lreg(next_operand_)] = operand_[next_operand_];
				MoveToState(Decode);
			}

			if(instruction_.operand_size() == InstructionSet::M68k::DataSize::LongWord) {
				MoveToState(StoreOperand_l);
			}

			SetupDataAccess(
				0,
				(instruction_.operand_size() == InstructionSet::M68k::DataSize::Byte) ? Microcycle::SelectByte : Microcycle::SelectWord);
			SetDataAddress(effective_address_[next_operand_]);
			Access(operand_[next_operand_].low);		// nw
		MoveToState(Decode);

		BeginState(StoreOperand_l):
			SetupDataAccess(0, Microcycle::SelectWord);
			SetDataAddress(effective_address_[next_operand_]);
			Access(operand_[next_operand_].low);		// nw

			effective_address_[next_operand_] -= 2;
			Access(operand_[next_operand_].high);		// nW
		MoveToState(Decode);

		//
		// Various generic forms of perform.
		//
#define MoveToWritePhase()														\
	next_operand_ = operand_flags_ >> 3;										\
	if(operand_flags_ & 0x0c) MoveToState(StoreOperand) else MoveToState(Decode)

		BeginState(Perform_np):
			InstructionSet::M68k::perform<InstructionSet::M68k::Model::M68000>(
				instruction_, operand_[0], operand_[1], status_, *static_cast<ProcessorBase *>(this));
			Prefetch();			// np
		MoveToWritePhase();

		BeginState(Perform_np_n):
			InstructionSet::M68k::perform<InstructionSet::M68k::Model::M68000>(
				instruction_, operand_[0], operand_[1], status_, *static_cast<ProcessorBase *>(this));
			Prefetch();			// np
			IdleBus(1);			// n
		MoveToWritePhase();

		BeginState(Perform_np_nn):
			InstructionSet::M68k::perform<InstructionSet::M68k::Model::M68000>(
				instruction_, operand_[0], operand_[1], status_, *static_cast<ProcessorBase *>(this));
			Prefetch();			// np
			IdleBus(2);			// nn
		MoveToWritePhase();

#undef MoveToWritePhase


		//
		// Specific forms of perform.
		//

		BeginState(MOVEw):
			switch(instruction_.mode(1)) {
				case Mode::DataRegisterDirect:
				case Mode::AddressRegisterDirect:
				MoveToState(MOVEwRegisterDirect);

				case Mode::AddressRegisterIndirectWithPostincrement:
				MoveToState(MOVEwAddressRegisterIndirectWithPostincrement);

				default: assert(false);
			}


		BeginState(MOVEwRegisterDirect):
			registers_[instruction_.lreg(1)].w = operand_[1].w;
			Prefetch();		// np
		MoveToState(Decode);

		BeginState(MOVEwAddressRegisterIndirectWithPostincrement):
			// TODO: nw
			assert(false);
			Prefetch()		// np
		MoveToState(Decode);

		// Various states TODO.
#define TODOState(x)	\
		BeginState(x): [[fallthrough]];

//		TODOState(FetchImmediateData_l);

#undef TODOState

		default:
			printf("Unhandled state: %d; opcode is %04x\n", state_, opcode_);
			assert(false);
	}}

#undef Prefetch
#undef ReadProgramWord
#undef ReadDataWord
#undef AccessPair
#undef CompleteAccess
#undef WaitForDTACK
#undef IdleBus
#undef PerformBusOperation
#undef MoveToState
#undef CheckOverrun
#undef Spend
#undef ConsiderExit

}

// MARK: - Flow Controller.

void ProcessorBase::did_update_status() {
	// Shuffle the stack pointers.
	stack_pointers_[is_supervisor_] = registers_[15];
	registers_[15] = stack_pointers_[int(status_.is_supervisor)];
	is_supervisor_ = int(status_.is_supervisor);
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


}
}

#endif /* _8000Mk2Implementation_h */
