//
//  State.h
//  Clock Signal
//
//  Created by Thomas Harte on 02/04/2020.
//  Copyright © 2020 Thomas Harte. All rights reserved.
//

#ifndef State_h
#define State_h

#include "../../../Reflection/Enum.hpp"
#include "../../../Reflection/Struct.hpp"
#include "../6502.hpp"

namespace CPU {
namespace MOS6502 {

/*!
	Provides a means for capturing or restoring complete 6502 state.

	This is an optional adjunct to the 6502 class. If you want to take the rest of the 6502
	implementation but don't want any of the overhead of my sort-of half-reflection as
	encapsulated in Reflection/[Enum/Struct].hpp just don't use this class.
*/
struct State: public Reflection::StructImpl<State> {
	/// Instantiates a new State based on the processor @c src.
	State(const ProcessorBase &src);

	/// Applies this state to @c target.
	void apply(ProcessorBase &target);

	/*!
		Provides the current state of the well-known, published internal registers.
	*/
	struct Registers: public Reflection::StructImpl<Registers> {
		uint16_t program_counter;
		uint8_t stack_pointer;
		uint8_t flags;
		uint8_t a, x, y;

		Registers();
	} registers;

	/*!
		Provides the current state of the processor's various input lines that aren't
		related to an access cycle.
	*/
	struct Inputs: public Reflection::StructImpl<Inputs> {
		bool ready;
		bool irq;
		bool nmi;
		bool reset;

		Inputs();
	} inputs;

	/*!
		Contains internal state used by this particular implementation of a 6502. Most of it
		does not necessarily correlate with anything in a real 6502, and some of it very
		obviously doesn't.
	*/
	struct ExecutionState: public Reflection::StructImpl<ExecutionState> {
		ReflectableEnum(Phase,
			Instruction, Stopped, Waiting, Jammed, Ready
		);

		/// Current executon phase, e.g. standard instruction flow or responding to an IRQ.
		Phase phase;
		int micro_program;
		int micro_program_offset;

		// The following are very internal things. At the minute I
		// consider these 'reliable' for inter-launch state
		// preservation only on the grounds that this implementation
		// of a 6502 is now empirically stable.
		//
		// If cycles_into_phase is 0, the values below need not be
		// retained, they're entirely ephemeral. If providing a state
		// for persistance, machines that can should advance until
		// cycles_into_phase is 0.
		uint8_t operation, operand;
		uint16_t address, next_address;

		ExecutionState();
	} execution_state;

	State() {
		if(needs_declare()) {
			DeclareField(registers);
			DeclareField(execution_state);
			DeclareField(inputs);
		}
	}
};

// Boilerplate follows here, to establish 'reflection' for the state struct defined above.
inline State::Registers::Registers() {
	if(needs_declare()) {
		DeclareField(program_counter);
		DeclareField(stack_pointer);
		DeclareField(flags);
		DeclareField(a);
		DeclareField(x);
		DeclareField(y);
	}
}

inline State::ExecutionState::ExecutionState() {
	if(needs_declare()) {
		AnnounceEnum(Phase);
		DeclareField(phase);
		DeclareField(micro_program);
		DeclareField(micro_program_offset);
		DeclareField(operation);
		DeclareField(operand);
		DeclareField(address);
		DeclareField(next_address);
	}
}

inline State::Inputs::Inputs() {
	if(needs_declare()) {
		DeclareField(ready);
		DeclareField(irq);
		DeclareField(nmi);
		DeclareField(reset);
	}
}

}
}

#endif /* State_h */
