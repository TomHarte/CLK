//
//  Perform.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 05/10/2023.
//  Copyright © 2023 Thomas Harte. All rights reserved.
//

#ifndef Perform_h
#define Perform_h

#include "Instruction.hpp"

namespace InstructionSet::x86 {

/// Performs @c instruction using @c resolver to obtain to query @c registers and/or @c memory as required, using @c io for port input/output,
/// and providing any flow control effects to @c flow_controller.
///
/// Any change in processor status will be applied to @c status.
///
/// If the template parameter @c operation is not @c Operation::Undefined then that operation will be performed, ignoring
/// whatever is specifed in @c instruction.
template <
	Model model,
	typename FlowControllerT,
	typename DataPointerResolverT,
	typename RegistersT,
	typename MemoryT,
	typename IOT,
	Operation operation = Operation::Undefined
> void perform(
	const Instruction &instruction,
	Status &status,
	FlowControllerT &flow_controller,
	DataPointerResolverT &resolver,
	RegistersT &registers,
	MemoryT &memory,
	IOT &io
);

}

#endif /* Perform_h */
