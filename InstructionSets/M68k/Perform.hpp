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
	void raise_exception(int)	{}
	void consume_cycles(int)	{}
};

/// Performs @c instruction using @c source and @c dest (which mmay be ignored as per the semantics of the operation).
/// And change in processor status will be applied to @c status. If this operation raises an exception, causes a
/// branch, or consumes additional cycles due to the particular value of the operands (on the 68000, think DIV or MUL),
/// that'll be notified to the @c flow_controller.
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
