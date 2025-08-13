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

#include <cassert>
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
	auto operator<=>(const DescriptorBounds &) const = default;
};

enum class DescriptorType {
	Code, Data, Stack,
	CallGate, TaskGate, InterruptGate, TrapGate,
	AvailableTaskStateSegment, LDT, BusyTaskStateSegment,
	Invalid,
};

constexpr bool is_data_or_code(const DescriptorType type) {
	return type <= DescriptorType::Stack;
}

enum DescriptorTypeFlag: uint8_t {
	Accessed	= 1 << 0,
	Busy		= 1 << 1,
};

struct DescriptorDescription {
	DescriptorType type = DescriptorType::Invalid;
	bool readable = false;
	bool writeable = false;
	bool conforming = false;
	bool is32bit = false;
};

struct SegmentDescriptor {
	SegmentDescriptor() = default;

	/// Creates a new descriptor with four 16-bit from a descriptor table.
	SegmentDescriptor(
		const uint16_t segment,
		const bool local,
		const uint16_t descriptor[4]
	) noexcept : segment_(segment), local_(local) {
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

	uint16_t segment() const {
		return segment_;
	}

	/// @returns The linear address for offest @c address within the segment described by this descriptor.
	uint32_t to_linear(const uint32_t address) const {
		return base_ + address;
	}

	void throw_gpf() const {
		throw Exception::exception<Vector::GeneralProtectionFault>(
			ExceptionCode(
				segment_,
				local_,
				false,
				false
			)
		);
	}

	template <AccessType type, typename AddressT>
	requires std::same_as<AddressT, uint16_t> || std::same_as<AddressT, uint32_t>
	void authorise(const AddressT begin, const AddressT end) const {
		// Test for bounds; end && end < begin captures instances where end is
		// both out of bounds and beyond the range of AddressT.
		if(begin < bounds_.begin || end > bounds_.end || (end && end < begin)) {
			throw_gpf();
		}

		// Tested at loading (?): present(), privilege_level().
		const auto desc = description();
		if(type == AccessType::Read && !desc.readable) {
			throw_gpf();
		}

		if(type == AccessType::Write && !desc.writeable) {
			throw_gpf();
		}
	}

	void validate_as(const Source segment) const {
		const auto desc = description();
		switch(segment) {
			case Source::DS:
			case Source::ES:
				if(!desc.readable) {
					printf("TODO: throw for unreadable DS or ES source.\n");
					assert(false);
				}
			break;

			case Source::SS:
				if(!desc.writeable) {
					printf("TODO: throw for unwriteable SS target.\n");
					assert(false);
				}
			break;

			default: break;
		}

		// TODO: is this descriptor privilege within reach?
		// TODO: is this an empty descriptor*? If so: exception!
	}

	// TODO: validators for:
	//	INT
	//	IRET
	//	JMP
	//	RET
	//
	// Verify also: MOV, POP, both of which can mutate DS/ES, SS, etc.

	void validate_call(
		const std::function<void(const SegmentDescriptor &)> &call_callback
	) const {
		const auto desc = description();
		switch(desc.type) {
			case DescriptorType::Code:
				if(desc.conforming) {
					// TODO:
					// DPL must be :5 CPL else #GP (code segment selector)
				} else {

				}

				call_callback(*this);
			break;

			case DescriptorType::CallGate:
				assert(false);
			break;

			case DescriptorType::AvailableTaskStateSegment:
				assert(false);
			break;

			default:
				throw_gpf();
			break;
		}
	}

	/// @returns The base of this segment descriptor.
	uint32_t base() const {		return base_;	}

	/// @returns The offset of this segment descriptor.
	uint32_t offset() const {	return offset_;	}

	/// @returns The bounds of this segment descriptor; will be either [0, limit] or [limit, INT_MAX] depending on descriptor type.
	/// Accesses must be `>= bounds().begin` and `<= bounds().end`.
	DescriptorBounds bounds() const {	return bounds_;	}

	bool present() const 			{	return type_ & 0x80;		}
	int privilege_level() const		{	return (type_ >> 5) & 3;	}
	uint8_t access_rights() const	{	return uint8_t(type_);		}

	DescriptorDescription description() const {
		using Type = DescriptorType;
		switch(type_ & 0b11111) {
			default:
			case 0b00000:    return { .type = Type::Invalid };
			case 0b00001:    return { .type = Type::AvailableTaskStateSegment, .is32bit = false };
			case 0b00010:    return { .type = Type::LDT };
			case 0b00011:    return { .type = Type::BusyTaskStateSegment, .is32bit = false };

			case 0b00100:    return { .type = Type::CallGate, .is32bit = false };
			case 0b00101:    return { .type = Type::TaskGate };
			case 0b00110:    return { .type = Type::InterruptGate, .is32bit = false };
			case 0b00111:    return { .type = Type::TrapGate, .is32bit = false };

			case 0b01000:    return { .type = Type::Invalid };
			case 0b01001:    return { .type = Type::AvailableTaskStateSegment, .is32bit = true };
			case 0b01010:    return { .type = Type::Invalid };
			case 0b01011:    return { .type = Type::BusyTaskStateSegment, .is32bit = true };

			case 0b01100:    return { .type = Type::CallGate, .is32bit = true };
			case 0b01101:    return { .type = Type::Invalid };
			case 0b01110:    return { .type = Type::InterruptGate, .is32bit = true };
			case 0b01111:    return { .type = Type::TrapGate, .is32bit = true };

			// b0 is the accessed flag for non-system descriptors; it doesn't affect the type.
			case 0b10000:
			case 0b10001:    return { .type = Type::Data, .readable = true, .writeable = false };
			case 0b10010:
			case 0b10011:    return { .type = Type::Data, .readable = true, .writeable = true };
			case 0b10100:
			case 0b10101:    return { .type = Type::Stack, .readable = true, .writeable = false };
			case 0b10110:
			case 0b10111:    return { .type = Type::Stack, .readable = true, .writeable = true };

			case 0b11000:
			case 0b11001:    return { .type = Type::Code, .readable = false, .writeable = false, .conforming = false };
			case 0b11010:
			case 0b11011:    return { .type = Type::Code, .readable = true, .writeable = false, .conforming = false };
			case 0b11100:
			case 0b11101:    return { .type = Type::Code, .readable = false, .writeable = false, .conforming = true };
			case 0b11110:
			case 0b11111:    return { .type = Type::Code, .readable = true, .writeable = false, .conforming = true };
		}
	}

	bool operator ==(const SegmentDescriptor &rhs) const {
		return
			base_ == rhs.base_ &&
			offset_ == rhs.offset_ &&
			bounds_ == rhs.bounds_ &&
			type_ == rhs.type_ &&
			segment_ == rhs.segment_;
	}

private:
	uint32_t base_;
	uint32_t offset_;
	DescriptorBounds bounds_;
	uint8_t type_;
	uint16_t segment_;
	bool local_;
};

struct InterruptDescriptor {
	InterruptDescriptor(const uint16_t, bool, const uint16_t descriptor[4]) noexcept :
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
	std::array<SegmentT, 6> values_{};
	static constexpr size_t index_of(const Source segment) {
		assert(is_segment_register(segment));
		return size_t(segment) - size_t(Source::ES);
	}
};

template <typename DescriptorT, typename LinearMemoryT>
//requires is_linear_memory<LinearMemoryT>
DescriptorT descriptor_at(
	LinearMemoryT &memory,
	const DescriptorTablePointer table,
	const uint16_t offset,
	const bool local
) {
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

	return DescriptorT(uint16_t(offset) & ~7, local, entry);
}

template <typename DescriptorT, typename LinearMemoryT>
//requires is_linear_memory<LinearMemoryT>
void set_descriptor_type_flag(
	LinearMemoryT &memory,
	const DescriptorTablePointer table,
	const DescriptorT &descriptor,
	const DescriptorTypeFlag flag
) {
	const auto address = table.base + (descriptor.segment() & ~7);
	const uint32_t table_end = table.base + table.limit;

	auto type = memory.template access<uint16_t, AccessType::PreauthorisedRead>(address + 5, table_end);
	type |= flag;
	memory.template access<uint16_t, AccessType::Write>(address + 5, table_end) = type;
}

}
