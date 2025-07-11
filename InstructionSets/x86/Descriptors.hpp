//
//  Descriptors.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 19/03/2025.
//  Copyright © 2025 Thomas Harte. All rights reserved.
//

#pragma once

#include "Exceptions.hpp"
#include "Instruction.hpp"
//#include "Perform.hpp"

#include <concepts>

namespace InstructionSet::x86 {

enum class DescriptorTable {
	Global, Local, Interrupt,
};

struct DescriptorTablePointer {
	uint16_t limit;
	uint32_t base;
};

struct DescriptorBounds {
	uint32_t begin, end;
};

enum class DescriptorType {
	Code, Data, Stack,
	CallGate, TaskGate, InterruptGate, TrapGate,
	AvailableTaskStateSegment, LDTDescriptor, BusyTaskStateSegment,
	Invalid,
};

struct DescriptorDescription {
	DescriptorType type = DescriptorType::Invalid;
	bool readable = false;
	bool writeable = false;
	bool conforming = false;
};


struct SegmentDescriptor {
	SegmentDescriptor() = default;

	/// Creates a new descriptor with four 16-bit from a descriptor table.
	SegmentDescriptor(const uint16_t segment, const uint16_t descriptor[4]) noexcept : segment_(segment) {
		base_ = uint32_t(descriptor[1] | ((descriptor[2] & 0xff) << 16));
		type_ = descriptor[2] >> 8;

		offset_ = descriptor[0];
		if(description().type != DescriptorType::Stack) {
			bounds_ = DescriptorBounds{ 0, offset_ };
		} else {
			if(offset_ != std::numeric_limits<uint32_t>::max()) {
				bounds_ = DescriptorBounds{ uint32_t(offset_ + 1), std::numeric_limits<uint32_t>::max() };
			} else {
				// This descriptor is impossible to satisfy for reasons that aren't
				// properly expressed if the lower bound is incremented, so make it
				// impossible to satisfy in a more prosaic sense.
				bounds_ = DescriptorBounds{ 1, 0 };
			}
		}
	}

	/// Rewrites this descriptor as a real-mode segment.
	void set_segment(const uint16_t segment) {
		segment_ = segment;
		base_ = uint32_t(segment) << 4;
		bounds_ = DescriptorBounds{ 0x0000, 0xffff };
		offset_ = 0;
		type_ = 0b1'00'1'001'0;		// Present, privilege level 0, expand-up writeable data, unaccessed.
	}

	/// @returns The linear address for offest @c address within the segment described by this descriptor.
	uint32_t to_linear(const uint32_t address) const {
		return base_ + address;
	}

	template <AccessType type, typename AddressT>
	requires std::same_as<AddressT, uint16_t> || std::same_as<AddressT, uint32_t>
	void authorise(const AddressT begin, const AddressT end) const {
		const auto throw_exception = [&] {
			throw Exception::exception<Vector::GeneralProtectionFault>(
				ExceptionCode(
					segment_,
					true,	// LDT or GDT???
					false,
					false
				)
			);
		};

		// Tested at loading (?): present(), privilege_level().

		const auto desc = description();

		if(type == AccessType::Read && !desc.readable) {
			throw_exception();
		}

		if(type == AccessType::Write && !desc.writeable) {
			throw_exception();
		}

		if(begin < bounds_.begin || end >= bounds_.end) {
			throw_exception();
		}
	}

	/// @returns The base of this segment descriptor.
	uint32_t base() const {		return base_;	}

	/// @returns The offset of this segment descriptor.
	uint32_t offset() const {	return offset_;	}

	/// @returns The bounds of this segment descriptor; will be either [0, limit] or [limit, INT_MAX] depending on descriptor type.
	/// Accesses must be `>= bounds().begin` and `<= bounds().end`.
	DescriptorBounds bounds() const {	return bounds_;	}

	DescriptorDescription description() const {
		using Type = DescriptorType;
		switch((type_ >> 1) & 0b1111) {
			default:
			case 0b0000:	return { .type = Type::Invalid };
			case 0b0001:	return { .type = Type::AvailableTaskStateSegment };
			case 0b0010:	return { .type = Type::LDTDescriptor };
			case 0b0011:	return { .type = Type::BusyTaskStateSegment };

			case 0b0100:	return { .type = Type::CallGate };
			case 0b0101:	return { .type = Type::TaskGate };
			case 0b0110:	return { .type = Type::InterruptGate };
			case 0b0111:	return { .type = Type::TrapGate };

			case 0b1000:	return { .type = Type::Data, .readable = true, .writeable = false };
			case 0b1001:	return { .type = Type::Data, .readable = true, .writeable = true };
			case 0b1010:	return { .type = Type::Stack, .readable = true, .writeable = false };
			case 0b1011:	return { .type = Type::Stack, .readable = true, .writeable = true };

			case 0b1100:	return { .type = Type::Code, .readable = false, .writeable = false, .conforming = false };
			case 0b1101:	return { .type = Type::Code, .readable = true, .writeable = false, .conforming = false };
			case 0b1110:	return { .type = Type::Code, .readable = false, .writeable = false, .conforming = true };
			case 0b1111:	return { .type = Type::Code, .readable = true, .writeable = false, .conforming = true };
		}
	}

	bool present() const 			{	return type_ & 0x80;		}	// b7
	int privilege_level() const		{	return (type_ >> 5) & 3;	}	// b5 and b6
	bool code_or_data() const 		{	return type_ & 0x10;		}	// b4

private:
	uint32_t base_;
	uint32_t offset_;
	DescriptorBounds bounds_;
	uint8_t type_;
	uint16_t segment_;
};

struct InterruptDescriptor {
	InterruptDescriptor(const uint16_t, const uint16_t descriptor[4]) noexcept :
		segment_(descriptor[1]),
		offset_(uint32_t(descriptor[0] | (descriptor[3] << 16))),
		flags_(descriptor[2] >> 8) {}

	uint16_t segment() const { return segment_; }
	uint32_t offset() const { return offset_; }
	bool present() const { return flags_ & 0x80; }
	uint8_t priority() const { return (flags_ >> 5) & 3; }

	enum class Type {
		Task = 0x5,
		Interrupt16 = 0x6,	Trap16 = 0x7,
		Interrupt32 = 0xe,	Trap32 = 0xf,
	};
	Type type() const {
		return Type(flags_ & 0xf);
	}

private:
	uint16_t segment_;
	uint32_t offset_;
	uint8_t flags_;
};


template <typename SegmentT>
struct SegmentRegisterSet {
	SegmentT &operator[](const Source segment) {
		return values_[index_of(segment)];
	}

	const SegmentT &operator[](const Source segment) const {
		return values_[index_of(segment)];
	}

	bool operator ==(const SegmentRegisterSet<SegmentT> &rhs) const {
		return values_ == rhs.values_;
	}

private:
	std::array<SegmentT, 6> values_;
	static constexpr size_t index_of(const Source segment) {
		assert(is_segment_register(segment));
		return size_t(segment) - size_t(Source::ES);
	}
};

template <typename DescriptorT, typename LinearMemoryT>
//requires is_linear_memory<LinearMemoryT>
DescriptorT descriptor_at(LinearMemoryT &memory, const DescriptorTablePointer table, const uint16_t offset) {
	if(offset > table.limit - 8) {
		printf("TODO: descriptor table overrun exception.\n");
		assert(false);
	}
	const auto address = table.base + offset;

	using AccessType = InstructionSet::x86::AccessType;
	const uint32_t table_end = table.base + table.limit;
	const uint16_t entry[] = {
		memory.template access<uint16_t, AccessType::Read>(address, table_end),
		memory.template access<uint16_t, AccessType::Read>(address + 2, table_end),
		memory.template access<uint16_t, AccessType::Read>(address + 4, table_end),
		memory.template access<uint16_t, AccessType::Read>(address + 6, table_end)
	};

	return DescriptorT(uint16_t(offset) & ~7, entry);
}

}
