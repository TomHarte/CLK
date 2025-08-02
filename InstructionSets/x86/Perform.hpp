//
//  Perform.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 05/10/2023.
//  Copyright © 2023 Thomas Harte. All rights reserved.
//

#pragma once

#include "AccessType.hpp"
#include "Descriptors.hpp"
#include "Flags.hpp"
#include "Instruction.hpp"
#include "Mode.hpp"
#include "Model.hpp"

#include "Numeric/RegisterSizes.hpp"

#include <concepts>

namespace InstructionSet::x86 {

//
// Register interface requirements.
// Chicken out for now: require prescribed class.
//
template <typename RegistersT>
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

template <typename RegistersT, DescriptorTable table>
concept has_descriptor_table = requires(RegistersT registers) {
	{ registers.template set<table>(DescriptorTablePointer{}) } -> std::same_as<void>;
	{ registers.template get<table>() } -> std::same_as<const DescriptorTablePointer &>;
};

template <typename RegistersT>
concept has_msw = requires(RegistersT registers) {
	{ registers.set_msw(uint16_t{}) } -> std::same_as<void>;
	{ registers.msw() } -> std::same_as<uint16_t>;

	{ registers.set_ldtr(uint16_t{}) } -> std::same_as<void>;
	{ registers.ldtr() } -> std::same_as<uint16_t>;
};

template <typename RegistersT>
concept is_registers_protected =
	has_msw<RegistersT> &&
	has_descriptor_table<RegistersT, DescriptorTable::Global> &&
	has_descriptor_table<RegistersT, DescriptorTable::Local> &&
	has_descriptor_table<RegistersT, DescriptorTable::Interrupt>;

template <typename RegistersT>
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
	is_registers_16<RegistersT> &&
	(!has_protected_mode<model> || is_registers_protected<RegistersT>) &&
	(!has_32bit_instructions<model> || is_registers_32<RegistersT>);

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
// Memory subsystem requirements.
//
template <typename LinearMemoryT, Model model>
concept is_linear_memory = requires(LinearMemoryT memory) {
	{ memory.template read<uint16_t>(uint32_t{}) } -> std::same_as<uint16_t>;
	memory.preauthorise_read(uint32_t{}, uint32_t{});
	memory.preauthorise_write(uint32_t{}, uint32_t{});
};

template <typename SegmentedMemoryT, typename AddressT, typename IntT, AccessType type>
concept supports_segmented_access = requires(SegmentedMemoryT memory) {
	{ memory.template access<IntT, type>(Source{}, AddressT{}) } -> std::same_as<typename Accessor<IntT, type>::type>;
};

template <typename SegmentedMemoryT, typename IntT>
concept supports_write_back = requires(SegmentedMemoryT memory) {
	memory.template write_back<IntT>();
};

template <typename SegmentedMemoryT, typename AddressT, typename IntT>
concept supports_segmented_accesses =
	supports_segmented_access<SegmentedMemoryT, AddressT, IntT, AccessType::PreauthorisedRead> &&
	supports_segmented_access<SegmentedMemoryT, AddressT, IntT, AccessType::Read> &&
	supports_segmented_access<SegmentedMemoryT, AddressT, IntT, AccessType::ReadModifyWrite> &&
	supports_segmented_access<SegmentedMemoryT, AddressT, IntT, AccessType::Write>;

template <typename SegmentedMemoryT, Model model>
concept is_segmented_memory_16 =
	supports_segmented_accesses<SegmentedMemoryT, uint16_t, uint8_t> &&
	supports_segmented_accesses<SegmentedMemoryT, uint16_t, uint16_t> &&
	supports_write_back<SegmentedMemoryT, uint8_t> &&
	supports_write_back<SegmentedMemoryT, uint16_t>;

template <typename SegmentedMemoryT, Model model>
concept is_segmented_memory_32 =
	supports_segmented_accesses<SegmentedMemoryT, uint16_t, uint32_t> &&
	supports_segmented_accesses<SegmentedMemoryT, uint32_t, uint8_t> &&
	supports_segmented_accesses<SegmentedMemoryT, uint32_t, uint16_t> &&
	supports_segmented_accesses<SegmentedMemoryT, uint32_t, uint32_t> &&
	supports_write_back<SegmentedMemoryT, uint32_t>;

template <typename SegmentedMemoryT, typename AddressT, typename IntT>
concept supports_preauthorisations = requires(SegmentedMemoryT memory) {
	memory.preauthorise_stack_write(uint32_t{});
	memory.preauthorise_stack_read(uint32_t{});

	memory.preauthorise_read(Source{}, AddressT{}, uint32_t{});
	memory.preauthorise_write(Source{}, AddressT{}, uint32_t{});

	memory.template preauthorised_write<IntT>(Source{}, AddressT{}, IntT{});
};

template <typename SegmentedMemoryT, Model model>
concept supports_preauthorisation =
	supports_preauthorisations<SegmentedMemoryT, uint16_t, uint8_t> &&
	supports_preauthorisations<SegmentedMemoryT, uint16_t, uint16_t> &&
	(
		!has_32bit_instructions<model> ||
		(
			supports_preauthorisations<SegmentedMemoryT, uint16_t, uint32_t> &&
			supports_preauthorisations<SegmentedMemoryT, uint32_t, uint8_t> &&
			supports_preauthorisations<SegmentedMemoryT, uint32_t, uint16_t> &&
			supports_preauthorisations<SegmentedMemoryT, uint32_t, uint32_t>
		)
	);

template <typename SegmentedMemoryT, Model model>
concept is_segmented_memory =
	(!has_protected_mode<model> || supports_preauthorisation<SegmentedMemoryT, model>) &&
	is_segmented_memory_16<SegmentedMemoryT, model> &&
	(!has_32bit_instructions<model> || is_segmented_memory_32<SegmentedMemoryT, model>);

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
	control.mode();		// TODO: require return type.
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
void perform(const Instruction<type> &, ContextT &, uint32_t source_ip);

/// Performs an Exception, which includes those generated by external sources.
/// @c source_ip is unused if the exception is an instance of `Exception::interrupt` but is required for other internal faults.
template <
	typename ContextT
>
requires is_context<ContextT>
void fault(Exception, ContextT &, const uint32_t source_ip = 0);

}

#include "Implementation/PerformImplementation.hpp"
