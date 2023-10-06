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
#include "Model.hpp"
#include "Status.hpp"

namespace InstructionSet::x86 {

/// Performs @c instruction  querying @c registers and/or @c memory as required, using @c io for port input/output,
/// and providing any flow control effects to @c flow_controller.
///
/// Any change in processor status will be applied to @c status.
template <
	Model model,
	typename InstructionT,
	typename FlowControllerT,
	typename RegistersT,
	typename MemoryT,
	typename IOT
> void perform(
	const InstructionT &instruction,
	Status &status,
	FlowControllerT &flow_controller,
	RegistersT &registers,
	MemoryT &memory,
	IOT &io
);

/*template <
	Model model,
	Operation operation,
	DataSize data_size,
	typename FlowControllerT
> void perform(
	CPU::RegisterPair32 &destination,
	CPU::RegisterPair32 &source,
	Status &status,
	FlowControllerT &flow_controller
);

template <
	Model model,
	Operation operation,
	DataSize data_size,
	typename FlowControllerT
> void perform(
	CPU::RegisterPair16 &destination,
	CPU::RegisterPair16 &source,
	Status &status,
	FlowControllerT &flow_controller
);*/

}

#include "Implementation/PerformImplementation.hpp"

#endif /* Perform_h */
