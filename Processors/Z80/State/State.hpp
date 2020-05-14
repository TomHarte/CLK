//
//  State.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 13/05/2020.
//  Copyright © 2020 Thomas Harte. All rights reserved.
//

#ifndef State_hpp
#define State_hpp

#include "../../../Reflection/Enum.hpp"
#include "../../../Reflection/Struct.hpp"
#include "../Z80.hpp"

namespace CPU {
namespace Z80 {

/*!
	Provides a means for capturing or restoring complete Z80 state.

	This is an optional adjunct to the Z80 class. If you want to take the rest of the Z80
	implementation but don't want any of the overhead of my sort-of half-reflection as
	encapsulated in Reflection/[Enum/Struct].hpp just don't use this class.
*/
struct State: public Reflection::StructImpl<State> {
	/*!
		Provides the current state of the well-known, published internal registers.
	*/
	struct Registers: public Reflection::StructImpl<Registers> {
		uint8_t a;
		uint8_t flags;
		uint16_t bc, de, hl;
		uint16_t bcDash, deDash, hlDash;
		uint16_t ix, iy, ir;
		uint16_t program_counter, stack_pointer;
		uint16_t memptr;
		int interrupt_mode;
		bool iff1, iff2;

		Registers();
	} registers;

	/*!
		Provides the current state of the processor's various input lines that aren't
		related to an access cycle.
	*/
	struct Inputs: public Reflection::StructImpl<Inputs> {
		bool irq;
		bool nmi;
		bool bus_request;
		bool wait;

		Inputs();
	} inputs;

	/*!
		Contains internal state used by this particular implementation of a 6502. Most of it
		does not necessarily correlate with anything in a real 6502, and some of it very
		obviously doesn't.
	*/
	struct ExecutionState: public Reflection::StructImpl<ExecutionState> {
		bool is_halted;

		uint8_t requests;
		uint8_t last_requests;
		uint8_t temp8;
		uint8_t operation;
		uint16_t temp16;
		unsigned int flag_adjustment_history;
		uint16_t pc_increment;

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

#endif /* State_hpp */
