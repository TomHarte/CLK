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

template <
	Model model_,
	typename FlowControllerT,
	typename RegistersT,
	typename MemoryT,
	typename IOT
> struct ExecutionContext {
	FlowControllerT flow_controller;
	Flags flags;
	RegistersT registers;
	MemoryT memory;
	IOT io;
	static constexpr Model model = model_;
};

/// Performs @c instruction  querying @c registers and/or @c memory as required, using @c io for port input/output,
/// and providing any flow control effects to @c flow_controller.
///
/// Any change in processor status will be applied to @c status.
template <
	typename InstructionT,
	typename ContextT
> void perform(
	const InstructionT &instruction,
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
