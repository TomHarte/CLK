//
//  Memory.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 01/12/2023.
//  Copyright © 2023 Thomas Harte. All rights reserved.
//

#pragma once

#include "InstructionSets/x86/AccessType.hpp"
#include "InstructionSets/x86/Descriptors.hpp"
#include "InstructionSets/x86/Model.hpp"
#include "InstructionSets/x86/Registers.hpp"

#include "LinearMemory.hpp"
#include "Segments.hpp"

#include <algorithm>

namespace PCCompatible {

template <InstructionSet::x86::Model model, typename LinearMemoryT> struct ProgramFetcher {
	std::pair<const uint8_t *, size_t> next_code(
		const InstructionSet::x86::Registers<model> &registers,
		const Segments<model, LinearMemoryT> &segments,
		LinearMemoryT &linear_memory
	) const {
		const uint16_t ip = registers.ip();
		const auto &descriptor = segments.descriptors[InstructionSet::x86::Source::CS];
		const uint32_t start = descriptor.to_linear(ip) & (linear_memory.MaxAddress - 1);
		return std::make_pair(
			linear_memory.at(start),
			std::min<size_t>(linear_memory.MaxAddress - start, 1 + descriptor.bounds().end - ip)
		);
	}

	std::pair<const uint8_t *, size_t> start_code(
		const Segments<model, LinearMemoryT> &segments,
		LinearMemoryT &linear_memory
	) const {
		const auto &descriptor = segments.descriptors[InstructionSet::x86::Source::CS];
		const auto base = uint32_t(descriptor.base() & (linear_memory.MaxAddress - 1));
		return std::make_pair(
			linear_memory.at(base),
			std::min<size_t>(0x1'0000, 1 + descriptor.bounds().end - base)
		);
	}
};

template <InstructionSet::x86::Model model, typename LinearMemoryT> class SegmentedMemory;

template <typename LinearMemoryT>
class SegmentedMemory<InstructionSet::x86::Model::i8086, LinearMemoryT> {
private:
	static constexpr auto model = InstructionSet::x86::Model::i8086;

public:
	using AccessType = InstructionSet::x86::AccessType;

	SegmentedMemory(
		InstructionSet::x86::Registers<model> &registers,
		const Segments<model, LinearMemoryT> &segments,
		LinearMemoryT &linear_memory
	) :
		registers_(registers), segments_(segments), linear_memory_(linear_memory) {}

	//
	// Preauthorisation call-ins.
	//
	void preauthorise_stack_write(uint32_t) {}
	void preauthorise_stack_read(uint32_t) {}
	void preauthorise_read(InstructionSet::x86::Source, uint16_t, uint32_t) {}
	void preauthorise_write(InstructionSet::x86::Source, uint16_t, uint32_t) {}

	//
	// Access call-ins.
	//

	// Accesses an address based on segment:offset.
	template <typename IntT, AccessType type>
	typename InstructionSet::x86::Accessor<IntT, type>::type access(
		const InstructionSet::x86::Source segment,
		const uint16_t offset
	) {
		const auto &descriptor = segments_.descriptors[segment];
		return linear_memory_.template access<IntT, type>(descriptor.to_linear(offset), descriptor.base());
	}

	template <typename IntT>
	void write_back() {
		linear_memory_.template write_back<IntT>();
	}

	template <typename IntT>
	void preauthorised_write(
		const InstructionSet::x86::Source segment,
		const uint16_t offset,
		const IntT value
	) {
		const auto &descriptor = segments_.descriptors[segment];
		linear_memory_.template preauthorised_write<IntT>(descriptor.to_linear(offset), descriptor.base(), value);
	}

	//
	// Helpers for instruction fetch.
	//
	std::pair<const uint8_t *, size_t> next_code() const {
		return program_fetcher_.next_code(registers_, segments_, linear_memory_);
	}

	std::pair<const uint8_t *, size_t> start_code() const {
		return program_fetcher_.start_code(segments_, linear_memory_);
	}

private:
	InstructionSet::x86::Registers<model> &registers_;
	const Segments<model, LinearMemoryT> &segments_;
	LinearMemoryT &linear_memory_;
	ProgramFetcher<model, LinearMemoryT> program_fetcher_;
};

template <typename LinearMemoryT>
class SegmentedMemory<InstructionSet::x86::Model::i80286, LinearMemoryT> {
public:
	static constexpr auto model = InstructionSet::x86::Model::i80286;
	using Mode = InstructionSet::x86::Mode;
	using AccessType = InstructionSet::x86::AccessType;

	SegmentedMemory(
		InstructionSet::x86::Registers<model> &registers,
		const Segments<model, LinearMemoryT> &segments,
		LinearMemoryT &linear_memory
	) : registers_(registers), segments_(segments), linear_memory_(linear_memory) {}

	//
	// Preauthorisation call-ins.
	//
	void preauthorise_stack_write(const uint32_t size) {
		const auto &descriptor = segments_.descriptors[InstructionSet::x86::Source::SS];
		descriptor.template authorise<InstructionSet::x86::AccessType::Write, uint16_t>(
			uint16_t(registers_.sp() - size),
			uint16_t(registers_.sp())
		);
	}

	void preauthorise_stack_read(const uint32_t size) {
		const auto &descriptor = segments_.descriptors[InstructionSet::x86::Source::SS];
		descriptor.template authorise<InstructionSet::x86::AccessType::Read, uint16_t>(
			uint16_t(registers_.sp() - size),
			uint16_t(registers_.sp())
		);
	}

	void preauthorise_read(InstructionSet::x86::Source, uint16_t, uint32_t) {}
	void preauthorise_write(InstructionSet::x86::Source, uint16_t, uint32_t) {}

	// TODO: perform authorisation checks.

	//
	// Access call-ins.
	//

	// Accesses an address based on segment:offset.
	template <typename IntT, AccessType type>
	typename InstructionSet::x86::Accessor<IntT, type>::type access(
		const InstructionSet::x86::Source segment,
		const uint16_t offset
	) {
		const auto &descriptor = segments_.descriptors[segment];
		descriptor.template authorise<type, uint16_t>(offset, offset + sizeof(IntT));
		return linear_memory_.template access<IntT, type>(descriptor.to_linear(offset), descriptor.base());
	}

	template <typename IntT>
	void write_back() {
		linear_memory_.template write_back<IntT>();
	}

	template <typename IntT>
	void preauthorised_write(
		const InstructionSet::x86::Source segment,
		const uint16_t offset,
		const IntT value
	) {
		const auto &descriptor = segments_.descriptors[segment];
		descriptor.template authorise<InstructionSet::x86::AccessType::Write, uint16_t>(offset, offset + sizeof(IntT));
		linear_memory_.template preauthorised_write<IntT>(descriptor.to_linear(offset), descriptor.base(), value);
	}

	//
	// Mode selection.
	//
	void set_mode(const Mode mode) {
		mode_ = mode;
	}

	//
	// Helpers for instruction fetch.
	//
	std::pair<const uint8_t *, size_t> next_code() const {
		return program_fetcher_.next_code(registers_, segments_, linear_memory_);
	}

	std::pair<const uint8_t *, size_t> start_code() const {
		return program_fetcher_.start_code(segments_, linear_memory_);
	}

private:
	InstructionSet::x86::Registers<model> &registers_;
	const Segments<model, LinearMemoryT> &segments_;
	LinearMemoryT &linear_memory_;
	ProgramFetcher<model, LinearMemoryT> program_fetcher_;
	Mode mode_ = Mode::Real;
};

}
