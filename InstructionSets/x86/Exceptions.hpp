//
//  Interrupts.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 06/10/2023.
//  Copyright © 2023 Thomas Harte. All rights reserved.
//

#pragma once

namespace InstructionSet::x86 {

enum class Vector: uint8_t {
	//
	// Present on all devices.
	//
	DivideError					= 0,
	SingleStep					= 1,
	NMI							= 2,
	Breakpoint					= 3,
	Overflow					= 4,
	BoundRangeExceeded			= 5,

	//
	// Added by the 80286.
	//
	InvalidOpcode				= 6,
	DeviceNotAvailable			= 7,
	DoubleFault					= 8,
	CoprocessorSegmentOverrun	= 9,
	InvalidTSS					= 10,
	SegmentNotPresent			= 11,
	StackSegmentFault			= 12,
	GeneralProtectionFault		= 13,
	FloatingPointException		= 16,

	//
	// Added by the 80286.
	//
	PageFault					= 14,
	AlignmentCheck				= 17,
	MachineCheck				= 18,
};

constexpr bool has_error_code(const Vector vector) {
	switch(vector) {
		using enum Vector;

		case DivideError:
		case SingleStep:
		case NMI:
		case Breakpoint:
		case Overflow:
		case BoundRangeExceeded:
		case InvalidOpcode:
		case DeviceNotAvailable:
		case CoprocessorSegmentOverrun:
		case FloatingPointException:
			return false;

		case DoubleFault:
		case InvalidTSS:
		case SegmentNotPresent:
		case StackSegmentFault:
		case GeneralProtectionFault:
			return true;

		default:	// 386 exceptions; I don't know yet.
		break;
	}
	assert(false);
	return false;
}

constexpr bool posts_next_ip(const Vector vector) {
	switch(vector) {
		using enum Vector;

		default:
			return false;

		case SingleStep:
		case Breakpoint:
		case Overflow:
			return true;
	}
}

struct ExceptionCode {
	ExceptionCode() = default;
	ExceptionCode(
		const uint16_t index,
		const bool is_local,
		const bool is_interrupt,
		const bool was_external) noexcept :
			value_(
				index |
				(is_local ? 0x4 : 0x0) |
				(is_interrupt ? 0x2 : 0x0) |
				(was_external ? 0x1 : 0x0)
			) {}

		// i.e.:
		//	b3–b15: IDT/GDT/LDT entry
		//	b2: 1 => in LDT; 0 => in GDT;
		//	b1: 1 => in IDT, ignore b2; 0 => use b2;
		//	b0:
		//		1 => trigger was external to program code;
		//		0 => trigger was caused by the instruction described by the CS:IP that is on the stack.

	operator uint16_t() const {
		return value_;
	}

	static ExceptionCode zero() {
		return ExceptionCode();
	}

private:
	uint16_t value_ = 0;
};

struct Exception {
	ExceptionCode code{};			// Exception code to push to the stack if this is an internal
									// exception that provides a code and post_ip_as_code is `false`.
	uint8_t vector{};				// Will be equal to value of a `Vector` enum if internal.

	enum class CodeType: uint8_t {
		Internal,
		External,
	};
	CodeType code_type = CodeType::Internal;

	/// Generates an internal exception with no error code.
	template <Vector cause>
	requires (!has_error_code(cause))
	static constexpr Exception exception() {
		return Exception(uint8_t(cause));
	}

	/// Generates an internal exception with a specified error code.
	template <Vector cause>
	requires (has_error_code(cause))
	static constexpr Exception exception(const ExceptionCode code) {
		return Exception(uint8_t(cause), code);
	}

	/// Generates an externally-motivated exception (i.e. an interrupt).
	static constexpr Exception interrupt(const uint8_t vector) {
		return Exception(vector, CodeType::External);
	}

private:
	constexpr Exception(const uint8_t vector) noexcept : vector(vector) {}
	constexpr Exception(const uint8_t vector, const ExceptionCode code) noexcept : code(code), vector(vector){}
	constexpr Exception(const uint8_t vector, const CodeType code_type) noexcept :
		vector(vector), code_type(code_type) {}
};

static_assert(sizeof(Exception) <= 4);

}
