//
//  Interrupts.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 06/10/2023.
//  Copyright © 2023 Thomas Harte. All rights reserved.
//

#pragma once

namespace InstructionSet::x86 {

enum Interrupt: uint8_t {
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

constexpr bool has_error_code(const Interrupt interrupt) {
	switch(interrupt) {
		case Interrupt::DivideError:
		case Interrupt::SingleStep:
		case Interrupt::NMI:
		case Interrupt::Breakpoint:
		case Interrupt::Overflow:
		case Interrupt::BoundRangeExceeded:
		case Interrupt::InvalidOpcode:
		case Interrupt::DeviceNotAvailable:
		case Interrupt::CoprocessorSegmentOverrun:
		case Interrupt::FloatingPointException:
			return false;

		case Interrupt::DoubleFault:
		case Interrupt::InvalidTSS:
		case Interrupt::SegmentNotPresent:
		case Interrupt::StackSegmentFault:
		case Interrupt::GeneralProtectionFault:
			return true;

		default:	// 386 exceptions; I don't know yet.
		break;
	}
	assert(false);
}

constexpr bool posts_next_ip(const Interrupt interrupt) {
	switch(interrupt) {
		default:
			return false;

		case Interrupt::SingleStep:
		case Interrupt::Breakpoint:
		case Interrupt::Overflow:
			return false;
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

private:
	uint16_t value_ = 0;
};

struct Exception {
	uint8_t cause;
	bool internal = true;
	ExceptionCode code;

	Exception() = default;
	constexpr Exception(const Interrupt cause) noexcept : cause(cause) {}
	constexpr Exception(const uint8_t external_cause) noexcept : cause(external_cause), internal(false) {}
	constexpr Exception(const Interrupt cause, const ExceptionCode code) noexcept : cause(cause), code(code) {}
};

}
