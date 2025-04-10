//
//  Memory.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 01/12/2023.
//  Copyright © 2023 Thomas Harte. All rights reserved.
//

#pragma once

#include "InstructionSets/x86/AccessType.hpp"
#include "InstructionSets/x86/Model.hpp"

#include "LinearMemory.hpp"
#include "Registers.hpp"
#include "Segments.hpp"

#include <algorithm>

namespace PCCompatible {

// TODO: the following need to apply linear memory semantics, including potential A20 wrapping.
template <InstructionSet::x86::Model model> struct ProgramFetcher {
	std::pair<const uint8_t *, size_t> next_code(
		const Registers<model> &registers,
		const Segments<model> &segments,
		LinearMemory<model> &linear_memory
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
		const Segments<model> &segments,
		LinearMemory<model> &linear_memory
	) const {
		const auto &descriptor = segments.descriptors[InstructionSet::x86::Source::CS];
		const auto base = descriptor.base() & (linear_memory.MaxAddress - 1);
		return std::make_pair(
			linear_memory.at(base),
			std::min<size_t>(0x1'0000, 1 + descriptor.bounds().end - base)
		);
	}

};

template <InstructionSet::x86::Model model> class SegmentedMemory;

template <>
class SegmentedMemory<InstructionSet::x86::Model::i8086> {
private:
	static constexpr auto model = InstructionSet::x86::Model::i8086;

public:
	using AccessType = InstructionSet::x86::AccessType;

	SegmentedMemory(
		Registers<model> &registers,
		const Segments<model> &segments,
		LinearMemory<model> &linear_memory
	) :
		registers_(registers), segments_(segments), linear_memory_(linear_memory) {}

	//
	// Preauthorisation call-ins. Since only an 8088 is currently modelled, all accesses are implicitly authorised.
	//
	void preauthorise_stack_write(uint32_t) {}
	void preauthorise_stack_read(uint32_t) {}
	void preauthorise_read(InstructionSet::x86::Source, uint16_t, uint32_t) {}
	void preauthorise_read(uint32_t, uint32_t) {}
	void preauthorise_write(InstructionSet::x86::Source, uint16_t, uint32_t) {}
	void preauthorise_write(uint32_t, uint32_t) {}

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
		return linear_memory_.access<IntT, type>(descriptor.to_linear(offset), descriptor.base());
	}

	template <typename IntT>
	void write_back() {
		linear_memory_.write_back<IntT>();
	}

	template <typename IntT>
	void preauthorised_write(
		const InstructionSet::x86::Source segment,
		const uint16_t offset,
		const IntT value
	) {
		const auto &descriptor = segments_.descriptors[segment];
		linear_memory_.preauthorised_write<IntT>(descriptor.to_linear(offset), descriptor.base(), value);
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
	Registers<model> &registers_;
	const Segments<model> &segments_;
	LinearMemory<model> &linear_memory_;
	ProgramFetcher<model> program_fetcher_;
};

template <>
class SegmentedMemory<InstructionSet::x86::Model::i80286> {
public:
	static constexpr auto model = InstructionSet::x86::Model::i80286;
	using Mode = InstructionSet::x86::Mode;
	using AccessType = InstructionSet::x86::AccessType;

	SegmentedMemory(
		Registers<model> &registers,
		const Segments<model> &segments,
		LinearMemory<model> &linear_memory
	) : registers_(registers), segments_(segments), linear_memory_(linear_memory) {}

	//
	// Preauthorisation call-ins. Since only an 8088 is currently modelled, all accesses are implicitly authorised.
	//
	void preauthorise_stack_write(uint32_t) {}
	void preauthorise_stack_read(uint32_t) {}
	void preauthorise_read(InstructionSet::x86::Source, uint16_t, uint32_t) {}
	void preauthorise_read(uint32_t, uint32_t) {}
	void preauthorise_write(InstructionSet::x86::Source, uint16_t, uint32_t) {}
	void preauthorise_write(uint32_t, uint32_t) {}

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
		return linear_memory_.access<IntT, type>(descriptor.to_linear(offset), descriptor.base());
	}

	template <typename IntT>
	void write_back() {
		linear_memory_.write_back<IntT>();
	}

	template <typename IntT>
	void preauthorised_write(
		const InstructionSet::x86::Source segment,
		const uint16_t offset,
		const IntT value
	) {
		const auto &descriptor = segments_.descriptors[segment];
		linear_memory_.preauthorised_write<IntT>(descriptor.to_linear(offset), descriptor.base(), value);
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
	Registers<model> &registers_;
	const Segments<model> &segments_;
	LinearMemory<model> &linear_memory_;
	ProgramFetcher<model> program_fetcher_;
	Mode mode_ = Mode::Real;
};

//template <InstructionSet::x86::Model model>
//class SegmentedMemory {
//	using AccessType = InstructionSet::x86::AccessType;
//	using Mode = InstructionSet::x86::Mode;
//
//	// Constructor.
//	SegmentedMemory(
//		Registers<model> &registers,
//		const Segments<model> &segments,
//		LinearMemory<model> &linear_memory
//	) :
//		registers_(registers), segments_(segments), linear_memory_(linear_memory) {}
//
//	//
//	// Preauthorisation call-ins. Since only an 8088 is currently modelled, all accesses are implicitly authorised.
//	//
//	void preauthorise_stack_write([[maybe_unused]] uint32_t length) {}
//	void preauthorise_stack_read([[maybe_unused]] uint32_t length) {}
//	void preauthorise_read([[maybe_unused]] InstructionSet::x86::Source segment, [[maybe_unused]] uint16_t start, [[maybe_unused]] uint32_t length) {}
//	void preauthorise_read([[maybe_unused]] uint32_t start, [[maybe_unused]] uint32_t length) {}
//	void preauthorise_write([[maybe_unused]] InstructionSet::x86::Source segment, [[maybe_unused]] uint16_t start, [[maybe_unused]] uint32_t length) {}
//	void preauthorise_write([[maybe_unused]] uint32_t start, [[maybe_unused]] uint32_t length) {}
//
//	void set_mode(const Mode mode) {
//		mode_ = mode;
//	}
//
//	//
//	// Access call-ins.
//	//
//
//	// Accesses an address based on segment:offset.
//	template <typename IntT, AccessType type>
//	typename InstructionSet::x86::Accessor<IntT, type>::type access(
//		const InstructionSet::x86::Source segment,
//		const uint16_t offset
//	) {
//		const auto &descriptor = segments_.descriptors[segment];
//		return linear_memory_.access<IntT, type>(
//		, ds, ds)
//
//	}
//
//	//
//	// Direct read and write.
//	//
//	template <typename IntT>
//	void preauthorised_write(
//		const InstructionSet::x86::Source segment,
//		const uint16_t offset,
//		const IntT value
//	) {
//		linear_memory_.access<IntT, AccessType::Write>(
//
//		// Bytes can be written without further ado.
//		if constexpr (std::is_same_v<IntT, uint8_t>) {
//			memory[address(segment, offset) & 0xf'ffff] = value;
//			return;
//		}
//
//		// Words that straddle the segment end must be split in two.
//		if(offset == 0xffff) {
//			memory[address(segment, offset) & 0xf'ffff] = uint8_t(value & 0xff);
//			memory[address(segment, 0x0000) & 0xf'ffff] = uint8_t(value >> 8);
//			return;
//		}
//
//		const uint32_t target = address(segment, offset) & 0xf'ffff;
//
//		// Words that straddle the end of physical RAM must also be split in two.
//		if(target == 0xf'ffff) {
//			memory[0xf'ffff] = uint8_t(value & 0xff);
//			memory[0x0'0000] = uint8_t(value >> 8);
//			return;
//		}
//
//		// It's safe just to write then.
//		*reinterpret_cast<IntT *>(&memory[target]) = value;
//	}
//
//	template <typename IntT>
//	IntT preauthorised_read(
//		const InstructionSet::x86::Source segment,
//		const uint16_t offset
//	) {
//		// Bytes can be written without further ado.
//		if constexpr (std::is_same_v<IntT, uint8_t>) {
//			return memory[address(segment, offset) & 0xf'ffff];
//		}
//
//		// Words that straddle the segment end must be split in two.
//		if(offset == 0xffff) {
//			return IntT(
//				memory[address(segment, offset) & 0xf'ffff] |
//				memory[address(segment, 0x0000) & 0xf'ffff] << 8
//			);
//		}
//
//		const uint32_t target = address(segment, offset) & 0xf'ffff;
//
//		// Words that straddle the end of physical RAM must also be split in two.
//		if(target == 0xf'ffff) {
//			return IntT(
//				memory[0xf'ffff] |
//				memory[0x0'0000] << 8
//			);
//		}
//
//		// It's safe just to write then.
//		return *reinterpret_cast<IntT *>(&memory[target]);
//	}
//
//	//
//	// Helper for instruction fetch.
//	//
//	std::pair<const uint8_t *, size_t> next_code() const {
//		const uint32_t start =
//			segments_.descriptors[InstructionSet::x86::Source::CS].to_linear(registers_.ip()) & 0xf'ffff;
//		return std::make_pair(&memory[start], 0x10'000 - start);
//	}
//
//	std::pair<const uint8_t *, size_t> start_code() const {
//		return std::make_pair(memory.data(), 0x10'000);
//	}
//
//private:
//	Registers<model> &registers_;
//	const Segments<model> &segments_;
//	LinearMemory<model> &linear_memory_;
//	Mode mode_ = Mode::Real;
//
//	uint32_t address(const InstructionSet::x86::Source segment, const uint16_t offset) const {
//		return segments_.descriptors[segment].to_linear(offset);
//	}
//};

}
