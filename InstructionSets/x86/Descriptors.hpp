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
	Descriptor() = default;

	Descriptor(const uint16_t descriptor[4]) noexcept {
		printf("%04x %04x %04x %04x", descriptor[0], descriptor[1], descriptor[2], descriptor[3]);

		base_ = uint32_t(descriptor[1] | ((descriptor[2] & 0xff) << 16));
		limit_ = descriptor[0];

		printf(" -> %04x -> +%04x\n", base_, limit_);

		present_ = descriptor[2] & 0x8000;
		privilege_level_ = (descriptor[2] >> 13) & 3;

		// TODO: need to know more about the below.
		if(descriptor[2] & 0x1000) {
			executable_ = descriptor[2] & 0x800;

			if(executable_) {
				conforming_ = descriptor[2] & 0x400;

				readable_ = descriptor[2] & 0x200;
				writeable_ = true;
			} else {
				// expand down = descriptor[2] & 0x400;

				writeable_ = descriptor[2] & 0x200;
				readable_ = true;
			}
		} else {
			assert(false);
		}
	}

	void set_segment(const uint16_t segment) {
		base_ = uint32_t(segment) << 4;
		limit_ = 0xffff;

		present_ = true;
		readable_ = writeable_ = true;
	}

	uint32_t to_linear(const uint32_t address) const {
		return base_ + address;
	}
	uint32_t base() const {		return base_;	}
	uint32_t limit() const {	return limit_;	}

	int privilege_level() const {
		return privilege_level_;
	}

	bool readable() const 	{	return readable_;	}
	bool writeable() const	{	return writeable_;	}

private:
	uint32_t base_;
	uint32_t limit_;
	// TODO: permissions, type, etc.

	int privilege_level_;
	enum class Type {
		AvailableTaskStateSegment = 1,
		LDTDescriptor = 2,
		BusyTaskStateSegment = 3,

		Invalid0 = 0,	Invalid8 = 8,

		Control4 = 4,	Control5 = 5,	Control6 = 6,	Control7 = 7,

		Reserved9 = 9,	ReservedA = 10,	ReservedB = 11,	ReservedC = 12,
		ReservedD = 13,	ReservedE = 14,	ReservedF = 15,
	} type_;

	bool present_;
	bool readable_;
	bool writeable_;
	bool conforming_;
	bool executable_;
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
