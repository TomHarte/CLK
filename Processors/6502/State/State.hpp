//
//  State.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 02/04/2020.
//  Copyright © 2020 Thomas Harte. All rights reserved.
//

#ifndef MOS6502_State_hpp
#define MOS6502_State_hpp

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

	/// Default constructor; makes no guarantees as to field values beyond those given above.
	State();

	/// Instantiates a new State based on the processor @c src.
	State(const ProcessorBase &src);

	/// Applies this state to @c target.
	void apply(ProcessorBase &target);
};


}
}

#endif /* MOS6502_State_hpp */
