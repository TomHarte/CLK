//
//  Perform.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 05/10/2023.
//  Copyright © 2023 Thomas Harte. All rights reserved.
//

#pragma once

#include "Flags.hpp"
#include "Instruction.hpp"
#include "Mode.hpp"
#include "Model.hpp"
#include "Registers.hpp"

#include "Numeric/RegisterSizes.hpp"

#include <concepts>

namespace InstructionSet::x86 {

//
// Register interface requirements.
// Chicken out for now: require prescribed class.
//
template <typename RegistersT, Model model>
concept is_registers_16 = requires(RegistersT registers) {
	{ registers.al() } -> std::same_as<uint8_t &>;
	{ registers.ah() } -> std::same_as<uint8_t &>;
	{ registers.ax() } -> std::same_as<uint16_t &>;
	{ registers.axp() } -> std::same_as<CPU::RegisterPair16 &>;

	{ registers.bl() } -> std::same_as<uint8_t &>;
	{ registers.bh() } -> std::same_as<uint8_t &>;
	{ registers.bx() } -> std::same_as<uint16_t &>;

	{ registers.cl() } -> std::same_as<uint8_t &>;
	{ registers.ch() } -> std::same_as<uint8_t &>;
	{ registers.cx() } -> std::same_as<uint16_t &>;

	{ registers.dl() } -> std::same_as<uint8_t &>;
	{ registers.dh() } -> std::same_as<uint8_t &>;
	{ registers.dx() } -> std::same_as<uint16_t &>;

	{ registers.sp() } -> std::same_as<uint16_t &>;
	{ registers.bp() } -> std::same_as<uint16_t &>;
	{ registers.si() } -> std::same_as<uint16_t &>;
	{ registers.di() } -> std::same_as<uint16_t &>;

	{ registers.ip() } -> std::same_as<uint16_t &>;

	{ registers.es() } -> std::same_as<uint16_t &>;
	{ registers.cs() } -> std::same_as<uint16_t &>;
	{ registers.ds() } -> std::same_as<uint16_t &>;
	{ registers.ss() } -> std::same_as<uint16_t &>;
};

template <typename RegistersT, Model model>
concept is_registers_32 = requires(RegistersT registers) {
	{ registers.eax() } -> std::same_as<uint32_t &>;
	{ registers.ebx() } -> std::same_as<uint32_t &>;
	{ registers.ecx() } -> std::same_as<uint32_t &>;
	{ registers.edx() } -> std::same_as<uint32_t &>;

	{ registers.esi() } -> std::same_as<uint32_t &>;
	{ registers.edi() } -> std::same_as<uint32_t &>;
	{ registers.ebp() } -> std::same_as<uint32_t &>;
	{ registers.esp() } -> std::same_as<uint32_t &>;

	{ registers.eip() } -> std::same_as<uint32_t &>;

	{ registers.fs() } -> std::same_as<uint16_t &>;
	{ registers.gs() } -> std::same_as<uint16_t &>;
};

template <typename RegistersT, Model model>
concept is_registers =
	is_registers_16<RegistersT, model> &&
	(!has_32bit_instructions<model> || is_registers_32<RegistersT, model>);

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
	(!has_protected_mode<model> || has_descriptor_table_update<SegmentsT, model>);

//
//
//
template <typename LinearMemoryT, Model model>
concept is_linear_memory = requires(LinearMemoryT memory) {
	{ memory.template read<uint16_t>(uint32_t{}) } -> std::same_as<uint16_t>;
};

template <typename SegmentedMemoryT, Model model>
concept is_segmented_memory = true;

//requires(SegmentedMemoryT memory) {
	// TODO: express, somehow.
//};

//
// Flow controller requirements.
//
template <typename FlowControllerT, Model model>
concept is_flow_controller_16 = requires(FlowControllerT controller) {
	controller.template jump<uint16_t>(0);
	controller.template jump<uint16_t>(0, 0);
	controller.halt();
	controller.wait();
	controller.repeat_last();
};

template <typename FlowControllerT, Model model>
concept is_flow_controller_32 = requires(FlowControllerT controller) {
	controller.template jump<uint32_t>(0);
	controller.template jump<uint32_t>(0, 0);
};

template <typename FlowControllerT, Model model>
concept is_flow_controller =
	is_flow_controller_16<FlowControllerT, model> &&
	(!has_32bit_instructions<model> || is_flow_controller_32<FlowControllerT, model>);

//
// CPU control requirements.
//
template <typename CPUControlT, Model model>
concept is_cpu_control = requires(CPUControlT control) {
	control.set_mode(Mode{});
};

//
// Complete context requirements.
//
template <typename ContextT>
concept is_context_real = requires(ContextT context) {
	{ context.flags } -> std::same_as<InstructionSet::x86::Flags &>;
	{ context.registers } -> is_registers<ContextT::model>;
	{ context.segments } -> is_segments<ContextT::model>;
	{ context.memory } -> is_segmented_memory<ContextT::model>;
	{ context.linear_memory } -> is_linear_memory<ContextT::model>;
	{ context.flow_controller } -> is_flow_controller<ContextT::model>;
};

template <typename ContextT>
concept is_context_protected = requires(ContextT context) {
	{ context.cpu_control } -> is_cpu_control<ContextT::model>;
};

template <typename ContextT>
concept is_context =
	is_context_real<ContextT> &&
	(!has_protected_mode<ContextT::model> || is_context_protected<ContextT>);

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
