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

#include <concepts>

namespace InstructionSet::x86 {

//
// Register interface requirements.
//
template <typename RegistersT, Model model>
concept is_registers = true;

//
// Segment/descriptor interface requirements.
//
template <typename SegmentsT, Model model>
concept has_segment_update = requires(SegmentsT segments) {
	segments.did_update(Source{});
};

template <typename SegmentsT, Model model>
concept has_descriptor_table_update = requires(SegmentsT segments) {
	segments.did_update(DescriptorTable{});
};

template <typename SegmentsT, Model model>
concept is_segments =
	has_segment_update<SegmentsT, model> &&
	(!has_descriptor_tables<model> || has_descriptor_table_update<SegmentsT, model>);

//
//
//
template <typename LinearMemoryT, Model model>
concept is_linear_memory = true;

template <typename SegmentedMemoryT, Model model>
concept is_segmented_memory = true;

template <typename FlowControllerT, Model model>
concept is_flow_controller = requires(FlowControllerT controller) {
	controller.template jump<uint16_t>(uint16_t{});
	controller.template jump<uint16_t>(uint16_t{}, uint16_t{});
	controller.halt();
	controller.wait();
	controller.repeat_last();

	// TODO: if 32-bit, check for jump<uint32_t>, Somehow?
};

template <typename CPUControlT, Model model>
concept is_cpu_control = true;
// TODO: if 286 or better, cpu_control.set_mode(Mode{});

template <typename ContextT>
concept is_context = requires(ContextT context) {
	{ context.flags } -> std::same_as<InstructionSet::x86::Flags &>;
	{ context.registers } -> is_registers<ContextT::model>;
	{ context.segments } -> is_segments<ContextT::model>;
	{ context.memory } -> is_segmented_memory<ContextT::model>;
	{ context.linear_memory } -> is_linear_memory<ContextT::model>;
	{ context.flow_controller } -> is_flow_controller<ContextT::model>;

	// TODO: is < 286 or has is_cpu_control for .cpu_control.
};

/// Performs @c instruction  querying @c registers and/or @c memory as required, using @c io for port input/output,
/// and providing any flow control effects to @c flow_controller.
///
/// Any change in processor status will be applied to @c status.
template <
	InstructionType type,
	typename ContextT
>
requires is_context<ContextT>
void perform(
	const Instruction<type> &instruction,
	ContextT &context
);

/// Performs an x86 INT operation; also often used by other operations to indicate an error.
template <
	typename ContextT
>
requires is_context<ContextT>
void interrupt(
	int index,
	ContextT &context
);

}

#include "Implementation/PerformImplementation.hpp"
