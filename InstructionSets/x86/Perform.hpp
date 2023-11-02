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

/// Explains the type of access that `perform` intends to perform; is provided as a template parameter to whatever
/// the caller supplies as `MemoryT` and `RegistersT` when obtaining a reference to whatever the processor
/// intends to reference.
///
/// `perform` guarantees to validate all accesses before modifying any state, giving the caller opportunity to generate
/// any exceptions that might be applicable.
enum class AccessType {
	/// The requested value will be read from.
	Read,
	/// The requested value will be written to.
	Write,
	/// The requested value will be read from and then written to.
	ReadModifyWrite,
	/// The requested value has already been authorised for whatever form of access is now intended, so there's no
	/// need further to inspect. This is done e.g. by operations that will push multiple values to the stack to verify that
	/// all necessary stack space is available ahead of pushing anything, though each individual push will then result in
	/// a further `Preauthorised` access.
	PreauthorisedRead,
	PreauthorisedWrite,
};

template <
	Model model_,
	typename FlowControllerT,
	typename RegistersT,
	typename MemoryT,
	typename IOT
> struct ExecutionContext {
	FlowControllerT flow_controller;
	Status status;
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

template <
	typename ContextT
> void interrupt(
	int index,
	ContextT &context
);

}

#include "Implementation/PerformImplementation.hpp"

#endif /* Perform_h */
