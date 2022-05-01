//
//  Sequencer.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 29/04/2022.
//  Copyright © 2022 Thomas Harte. All rights reserved.
//

#ifndef InstructionSets_68k_Sequencer_hpp
#define InstructionSets_68k_Sequencer_hpp

#include "Instruction.hpp"
#include "Model.hpp"

namespace InstructionSet {
namespace M68k {

/// Additional guarantees: [Fetch/Store/CalcEA][1/2] have an LSB of 0 for
/// operand 1, and an LSB of 1 for operand 2.
enum class Step {
	/// No further steps remain.
	Done,
	/// Do the logical operation.
	Perform,
	/// Fetch the value of operand 1.
	FetchOp1,
	/// Fetch the value of operand 2.
	FetchOp2,
	/// Store the value of operand 1.
	StoreOp1,
	/// Store the value of operand 2.
	StoreOp2,
	/// Calculate effective address of operand 1.
	CalcEA1,
	/// Calculate effective address of operand 2.
	CalcEA2,
	/// A catch-all for bus activity that doesn't fit the pattern
	/// of fetch/stop operand 1/2, e.g. this opaquely covers almost
	/// the entirety of MOVEM.
	///
	/// TODO: list all operations that contain this step,
	/// and to cover what activity.
	SpecificBusActivity,

	Max = SpecificBusActivity
};

/// Indicates the abstract steps necessary to perform an operation,
/// at least as far as that's generic.
template<Model model> class Sequence {
	public:
		Sequence(Operation);

		/// @returns The next @c Step to perform, or @c Done
		/// if no further steps remain. This step is removed from the
		/// list of remaining steps.
		Step pop_front() {
			static_assert(int(Step::Max) < 16);
			const auto step = Step(steps_ & 15);
			steps_ >>= 4;
			return step;
		}

		/// @returns @c true if no steps other than @c Done remain;
		/// @c false otherwise.
		bool empty() {
			return !steps_;
		}

	private:
		uint32_t steps_ = 0;

		uint32_t steps_for(Operation);
};

static_assert(sizeof(Sequence<Model::M68000>) == sizeof(uint32_t));

}
}

#endif /* InstructionSets_68k_Sequencer_h */
