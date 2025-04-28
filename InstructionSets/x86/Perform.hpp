//
//  Perform.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 05/10/2023.
//  Copyright © 2023 Thomas Harte. All rights reserved.
//

#pragma once

#include "Instruction.hpp"
#include "Model.hpp"
#include "Flags.hpp"

namespace InstructionSet::x86 {


/// Performs @c instruction  querying @c registers and/or @c memory as required, using @c io for port input/output,
/// and providing any flow control effects to @c flow_controller.
///
/// Any change in processor status will be applied to @c status.
template <
	InstructionType type,
	typename ContextT
> void perform(
	const Instruction<type> &instruction,
	ContextT &context
);

/// Performs an x86 INT operation; also often used by other operations to indicate an error.
template <
	typename ContextT
> void interrupt(
	int index,
	ContextT &context
);

}

#include "Implementation/PerformImplementation.hpp"
