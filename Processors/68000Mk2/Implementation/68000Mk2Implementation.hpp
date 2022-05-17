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
#define MoveToState(x)	state_ = x; if (permit_overrun && time_remaining_ <= HalfCycles(0)) return

	//
	// So basic structure is, in general:
	//
	//	case Action:
	//		do_something();
	//		Spend(20);
	//		do_something_else();
	//		Spend(10);
	//		do_a_third_thing();
	//		Spend(30);
	//
	//		MoveToState(next_action);
	//		break;
	//
	// Additional notes:
	//
	//	Action and all equivalents should be negative values, since the
	//	switch-for-computed-goto-for-a-coroutine structure uses __COUNTER__* for
	//	its invented entry- and exit-points, meaning that negative numbers are
	//	the easiest group that is safely definitely never going to collide.
	//
	//	(* an extension supported by at least GCC, Clang and MSVC)


	// Some microcycles that will be modified as required and used in the loop below;
	// the semantics of a switch statement make in-place declarations awkward.
	Microcycle idle{0};

	// Read a data word.
	Microcycle read_word_data_announce {
		Microcycle::Read | Microcycle::NewAddress | Microcycle::IsData
	};
	Microcycle read_word_data {
		Microcycle::Read | Microcycle::SameAddress | Microcycle::SelectWord | Microcycle::IsData
	};

	// Read a program word. All accesses via the program counter are word sized.
	Microcycle read_program_announce {
		Microcycle::Read | Microcycle::NewAddress | Microcycle::IsProgram
	};
	Microcycle read_program {
		Microcycle::Read | Microcycle::SameAddress | Microcycle::SelectWord | Microcycle::IsProgram
	};

	// Holding spot when awaiting DTACK/etc.
	Microcycle awaiting_dtack;

	// Spare containers:
	uint32_t address = 0;	// For an address, where it isn't provideable by reference.
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
		state_ = State::WaitForDTACK;									\
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
#define AccessPair(addr, val, announce, perform)	\
	perform.address = announce.address = &addr;		\
	perform.value = &val;							\
	if constexpr (!dtack_is_implicit) {				\
		announce.length = HalfCycles(4);			\
	}												\
	PerformBusOperation(announce);					\
	WaitForDTACK(announce);							\
	CompleteAccess(perform);

	// Reads the data (i.e. non-program) word from addr into val.
#define ReadDataWord(addr, val)		AccessPair(addr, val, read_word_data_announce, read_word_data)

	// Reads the program (i.e. non-data) word from addr into val.
#define ReadProgramWord(val)															\
	AccessPair(program_counter_.l, val, read_program_announce, read_program);	\
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
		case State::WaitForDTACK:
			PerformBusOperation(awaiting_dtack);

			if(dtack_ || berr_ || vpa_) {
				state_ = post_dtack_state_;
			}
		break;

		// Perform the RESET exception, which seeds the stack pointer and program
		// counter, populates the prefetch queue, and then moves to instruction dispatch.
		case State::Reset:
			IdleBus(7);			// (n-)*5   nn

			address = 0;	ReadDataWord(address, registers_[15].high);		// nF
			address += 2;	ReadDataWord(address, registers_[15].low);		// nf
			address += 2;	ReadDataWord(address, program_counter_.high);	// nV
			address += 2;	ReadDataWord(address, program_counter_.low);	// nv

			Prefetch();			// np
			IdleBus(1);			// n
			Prefetch();			// np

			MoveToState(State::Decode);
		break;

		// Inspect the prefetch queue in order to decode the next instruction,
		// and segue into the fetching of operands.
		case State::Decode:
			opcode_ = prefetch_.high.w;
			instruction_ = decoder_.decode(opcode_);
			instruction_address_ = program_counter_.l - 4;

			// TODO: check for privilege and unrecognised instructions.

			// Obtain operand flags and pick a perform pattern.
			setup_operation();

			next_operand_ = 0;
		[[fallthrough]];

		// Check the operand flags to determine whether the operand at index
		// operand_ needs to be fetched, and do so.
		case State::FetchOperand:
			switch(instruction_.mode(next_operand_)) {
				case Mode::None:
					state_ = perform_state_;
				continue;

				case Mode::AddressRegisterDirect:
				case Mode::DataRegisterDirect:
					operand_[next_operand_] = registers_[instruction_.lreg(next_operand_)];
					++next_operand_;
					state_ = next_operand_ == 2 ? perform_state_ : state_;
				continue;

				default:
					assert(false);
			}

		[[fallthrough]];
		default:
			printf("Unhandled or unterminated state: %d\n", state_);
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

template <class BusHandler, bool dtack_is_implicit, bool permit_overrun, bool signal_will_perform>
void Processor<BusHandler, dtack_is_implicit, permit_overrun, signal_will_perform>::setup_operation() {

#define BIND(x, p)	\
	case InstructionSet::M68k::Operation::x:			\
		operand_flags_ = InstructionSet::M68k::operand_flags<InstructionSet::M68k::Model::M68000, InstructionSet::M68k::Operation::x>();	\
		perform_state_ = State::p;	\
	break;

	switch(instruction_.operation) {
		BIND(NBCD, Perform_np);

		default:
			assert(false);
	}

#undef BIND
}

template <class BusHandler, bool dtack_is_implicit, bool permit_overrun, bool signal_will_perform>
CPU::MC68000Mk2::State Processor<BusHandler, dtack_is_implicit, permit_overrun, signal_will_perform>::get_state() {
	return CPU::MC68000Mk2::State();
}

template <class BusHandler, bool dtack_is_implicit, bool permit_overrun, bool signal_will_perform>
void Processor<BusHandler, dtack_is_implicit, permit_overrun, signal_will_perform>::set_state(const CPU::MC68000Mk2::State &) {
}

}
}

#endif /* _8000Mk2Implementation_h */
