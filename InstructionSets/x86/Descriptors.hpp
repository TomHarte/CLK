//
//  Descriptors.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 19/03/2025.
//  Copyright © 2025 Thomas Harte. All rights reserved.
//

#pragma once

#include "Instruction.hpp"

namespace InstructionSet::x86 {

enum class DescriptorTable {
	Global, Local, Interrupt,
};

struct DescriptorTablePointer {
	uint16_t limit;
	uint32_t base;
};

struct Descriptor {
	void set_segment(const uint16_t segment) {
		base_ = uint32_t(segment) << 4;
		limit_ = std::numeric_limits<uint32_t>::max();
	}

	void set(uint64_t);

	// 286:
	// 47...32 = 16-bit limit;
	// 31 = P
	// 30...29 = DPL
	// 28 = S
	// 27...24 = type;
	// 23...00 = 4-bit base.

	uint32_t to_linear(const uint32_t address) const {
		return base_ + address;
	}

private:
	uint32_t base_;
	uint32_t limit_;
	// TODO: permissions, type, etc.
};

template <typename SegmentT>
struct SegmentRegisterSet {
	SegmentT &operator[](const Source segment) {
		return values_[index_of(segment)];
	}

	const SegmentT &operator[](const Source segment) const {
		return values_[index_of(segment)];
	}

private:
	std::array<SegmentT, 6> values_;
	static constexpr size_t index_of(const Source segment) {
		assert(is_segment_register(segment));
		return size_t(segment) - size_t(Source::ES);
	}
};

}
