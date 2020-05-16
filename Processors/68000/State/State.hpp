//
//  State.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 14/05/2020.
//  Copyright © 2020 Thomas Harte. All rights reserved.
//

#ifndef MC68000_State_hpp
#define MC68000_State_hpp

#include "../../../Reflection/Enum.hpp"
#include "../../../Reflection/Struct.hpp"
#include "../68000.hpp"

namespace CPU {
namespace MC68000 {

/*!
	Provides a means for capturing or restoring complete 68000 state.

	This is an optional adjunct to the 68000 class. If you want to take the rest of the 68000
	implementation but don't want any of the overhead of my sort-of half-reflection as
	encapsulated in Reflection/[Enum/Struct].hpp just don't use this class.
*/
struct State: public Reflection::StructImpl<State> {
	/*!
		Provides the current state of the well-known, published internal registers.
	*/
	struct Registers: public Reflection::StructImpl<Registers> {
		uint32_t data[8], address[7];
		uint32_t user_stack_pointer;
		uint32_t supervisor_stack_pointer;
		uint16_t status;
		uint32_t program_counter;
		uint32_t prefetch;

		Registers();
	} registers;

	/*!
		Provides the current state of the processor's various input lines that aren't
		related to an access cycle.
	*/
	struct Inputs: public Reflection::StructImpl<Inputs> {
		Inputs();
	} inputs;

	/*!
		Contains internal state used by this particular implementation of a 6502. Most of it
		does not necessarily correlate with anything in a real 6502, and some of it very
		obviously doesn't.
	*/
	struct ExecutionState: public Reflection::StructImpl<ExecutionState> {
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

#endif /* MC68000_State_hpp */
