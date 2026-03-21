//
//  Registers.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 25/02/2026.
//  Copyright © 2026 Thomas Harte. All rights reserved.
//

#pragma once

#include <cassert>
#include <cstdint>

#include "Numeric/RegisterSizes.hpp"
#include "InstructionSets/6809/OperationMapper.hpp"

namespace CPU::M6809 {

enum ConditionCode: uint8_t {
	Entire		= 0b1000'0000,
	FIRQMask	= 0b0100'0000,
	HalfCarry	= 0b0010'0000,
	IRQMask		= 0b0001'0000,
	Negative	= 0b0000'1000,
	Zero		= 0b0000'0100,
	Overflow	= 0b0000'0010,
	Carry		= 0b0000'0001,
};

struct ConditionCodeRegister {
	ConditionCodeRegister() = default;
	ConditionCodeRegister(const uint8_t value) {
		*this = value;
	}

	template <ConditionCode code>
	bool get() const {
		switch(code) {
			case ConditionCode::Entire:		return entire_;
			case ConditionCode::FIRQMask:	return firq_;
			case ConditionCode::HalfCarry:	return half_carry_;
			case ConditionCode::IRQMask:	return irq_;
			case ConditionCode::Negative:	return negative_ & 0x80;
			case ConditionCode::Zero:		return !zero_;
			case ConditionCode::Overflow:	return overflow_ & 0x80;
			case ConditionCode::Carry:		return carry_;
		}
	}

	template <ConditionCode code>
	void set(const bool value) {
		switch(code) {
			case ConditionCode::Entire:		entire_ = value;					break;
			case ConditionCode::FIRQMask:	firq_ = value;						break;
			case ConditionCode::HalfCarry:	half_carry_ = value ? 0x10 : 0x00;	break;
			case ConditionCode::IRQMask:	irq_ = value;						break;
			case ConditionCode::Negative:	negative_ = value ? 0xff : 0x00;	break;
			case ConditionCode::Zero:		zero_ = value ? 0x00 : 0xff;		break;
			case ConditionCode::Overflow:	overflow_ = value ? 0xff : 0x00;	break;
			case ConditionCode::Carry:		carry_ = value ? 0x01 : 0x00;		break;
		}
	}

	uint8_t operator =(const uint8_t rhs) {
		entire_ = rhs & ConditionCode::Entire;
		firq_ = rhs & ConditionCode::FIRQMask;
		half_carry_ = uint8_t((rhs & ConditionCode::HalfCarry) >> 1);
		irq_ = rhs & ConditionCode::IRQMask;
		negative_ = uint8_t((rhs & ConditionCode::Negative) << 4);
		zero_ = !(rhs & ConditionCode::Zero);
		overflow_ = uint8_t((rhs & ConditionCode::Overflow) << 6);
		carry_ = rhs & ConditionCode::Carry;

		assert(uint8_t(*this) == rhs);
		return rhs;
	}

	operator uint8_t() const {
		return uint8_t(
			(entire_ ? ConditionCode::Entire : 0) |
			(firq_ ? ConditionCode::FIRQMask : 0) |
			(half_carry_ << 1) |
			(irq_ ? ConditionCode::IRQMask : 0) |
			((negative_ & 0x80) ? ConditionCode::Negative : 0) |
			(zero_ ? 0 : ConditionCode::Zero) |
			((overflow_ & 0x80) ? ConditionCode::Overflow : 0) |
			carry_
		);
	}

	template <std::unsigned_integral ValueT>
	requires (sizeof(ValueT) <= 2)
	void set_nz(const ValueT value) {
		if constexpr (std::is_same_v<ValueT, uint8_t>) {
			negative_ = zero_ = value;
		} else {
			negative_ = uint8_t(value >> 8);
			zero_ = uint8_t(value | (value >> 8));
		}
	}

	template <std::unsigned_integral ValueT>
	requires (sizeof(ValueT) <= 2)
	void set_overflow(const ValueT result, const ValueT lhs, const ValueT rhs) {
		const ValueT bits = (result^lhs) & (result^rhs);
		if constexpr (std::is_same_v<ValueT, uint8_t>) {
			overflow_ = bits;
		} else {
			overflow_ = uint8_t(bits >> 8);
		}
	}

	uint8_t carry() const { return carry_; }
	uint8_t half_carry() const { return half_carry_; }

	template <InstructionSet::M6809::Condition condition>
	bool test() {
		switch(condition) {
			using enum InstructionSet::M6809::Condition;

			case A: return true;
			case N: return false;

			case CC: return	!get<ConditionCode::Carry>();
			case CS: return	get<ConditionCode::Carry>();
			case VC: return	!get<ConditionCode::Overflow>();
			case VS: return	get<ConditionCode::Overflow>();
			case PL: return	!get<ConditionCode::Negative>();
			case MI: return	get<ConditionCode::Negative>();
			case NE: return	!get<ConditionCode::Zero>();
			case EQ: return	get<ConditionCode::Zero>();

			case LS: return	get<ConditionCode::Zero>() || get<ConditionCode::Carry>();
			case LE: return	get<ConditionCode::Zero>() || test<InstructionSet::M6809::Condition::LT>();
			case LT: return	get<ConditionCode::Negative>() != get<ConditionCode::Overflow>();

			case GE: return !test<InstructionSet::M6809::Condition::LT>();
			case GT: return !test<InstructionSet::M6809::Condition::LE>();
			case HI: return !test<InstructionSet::M6809::Condition::LS>();
		}
		return false;
	}

private:
	uint8_t negative_ = 0;
	uint8_t zero_ = 0;
	uint8_t overflow_ = 0;
	uint8_t carry_ = 0;
	bool irq_ = true;
	uint8_t half_carry_ = 0;
	bool firq_ = true;
	bool entire_ = false;
};

enum class R8 {
	A,
	B,
	CC,
	DP,
};
enum class R16 {
	D,
	X,
	Y,
	S,
	U,
	PC,
};

struct Registers {
	uint16_t x;
	uint16_t y;
	uint16_t u;
	uint16_t s;
	RegisterPair16 pc;
	RegisterPair16 d;
	uint8_t dp;
	ConditionCodeRegister cc;

	template <R8 r>
	auto &reg() {
		if constexpr (r == R8::CC) {
			return cc;
		} else {
			switch(r) {
				case R8::A:		return d.halves.high;
				case R8::B:		return d.halves.low;
				case R8::DP:	return dp;
				default:	__builtin_unreachable();
			}
		}
	}

	template <R16 r>
	uint16_t &reg() {
		switch(r) {
			case R16::D:	return d.full;
			case R16::X:	return x;
			case R16::Y:	return y;
			case R16::S:	return s;
			case R16::U:	return u;
			case R16::PC:	return pc.full;
			default:	__builtin_unreachable();
		}
	}
};

struct IndexedAddressDecoder {
	IndexedAddressDecoder() = default;
	constexpr IndexedAddressDecoder(const uint8_t format) noexcept : format_(format) {}

	enum class FormSuffix {
		NoOffset = 0b0100,
		Extended = 0b1111,

		Offset8bit = 0b1000,
		Offset16bit = 0b1001,
		Offset8bitFromPC = 0b1100,
		Offset16bitFromPC = 0b1101,

		ARegisterOffset = 0b0110,
		BRegisterOffset = 0b0101,
		DRegisterOffset = 0b1011,

		PostincrementBy1 = 0b0000,
		PostincrementBy2 = 0b0001,
		PredecrementBy1 = 0b0010,
		PredecrementBy2 = 0b0011,
	};

	constexpr FormSuffix suffix() const {
		return FormSuffix(format_ & 0b1111);
	}
	constexpr bool is_5bit() const {
		return !(format_ & 0b1000'0000);
	}
	constexpr bool indirect() const {
		return
			suffix() != FormSuffix::PostincrementBy1 &&
			suffix() != FormSuffix::PredecrementBy1 &&
			(format_ & 0b1001'0000) == 0b1001'0000;
	}

	constexpr int required_continuation() const {
		if(is_5bit()) return 0;
		switch(suffix()) {
			using enum FormSuffix;
			case Offset8bit:
			case Offset8bitFromPC:	return 1;

			case Offset16bit:
			case Offset16bitFromPC:
			case Extended: return 2;

			default: return 0;
		}
	}

	void set_continuation(const uint16_t continuation) {
		continuation_ = continuation;
	}

	// Evaluates the address implied by this indexed address, assuming all required continuation bytes have
	// been provided.
	//
	// Will apply any automatic increment or decrement to the registers.
	uint16_t address(Registers &registers) const {
		const auto reg = [&] () -> uint16_t & {
			switch((format_ >> 5) & 0b11) {
				case 0b00:	return registers.reg<R16::X>();
				case 0b01:	return registers.reg<R16::Y>();
				case 0b10:	return registers.reg<R16::U>();
				case 0b11:	return registers.reg<R16::S>();
				default: __builtin_unreachable();
			}
		};

		const uint16_t base = [&]() -> uint16_t {
			if(is_5bit()) {
				return reg();
			}

			switch(suffix()) {
				using enum FormSuffix;
				default: return reg();

				case Extended:
					return 0;

				case Offset8bitFromPC:
				case Offset16bitFromPC:
					return registers.reg<R16::PC>();

				case PredecrementBy2:
					reg() -= 2;
					return reg();

				case PredecrementBy1:
					reg() -= 1;
					return reg();

				case PostincrementBy1: {
					const uint16_t result = reg();
					reg() += 1;
					return result;
				}

				case PostincrementBy2: {
					const uint16_t result = reg();
					reg() += 2;
					return result;
				}
			}
		} ();

		// A high bit of 0 implies the low five bits are the offset.
		if(is_5bit()) {
			// Shift sign bit up to bit 7 and convert to int8_t to effect it.
			// Then shift back down to get the correct value.
			return uint16_t(base + (int8_t(format_ << 3) >> 3));
		}

		switch(suffix()) {
			using enum FormSuffix;

			default:	return base;

			case Offset8bit:		return uint16_t(base + int8_t(continuation_));
			case Offset16bit:		return uint16_t(base + continuation_);
			case Offset8bitFromPC:	return uint16_t(registers.reg<R16::PC>() + int8_t(continuation_));
			case Offset16bitFromPC:	return uint16_t(registers.reg<R16::PC>() + continuation_);

			case ARegisterOffset:	return uint16_t(base + int8_t(registers.reg<R8::A>()));
			case BRegisterOffset:	return uint16_t(base + int8_t(registers.reg<R8::B>()));
			case DRegisterOffset:	return base + registers.reg<R16::D>();

			case Extended:			return continuation_;
		}

		return base;
	}

	/// @returns The number of cycles spent internally calculating the effective address.
	int address_cost() const {
		if(is_5bit()) {
			return 1;
		}
		switch(suffix()) {
			using enum FormSuffix;
			default:	// For invalid suffixes.
			case NoOffset:			return 0;

			case Offset8bit:		return 1;
			case Offset16bit:		return 3;

			case ARegisterOffset:	return 1;
			case BRegisterOffset:	return 1;
			case DRegisterOffset:	return 4;

			case PostincrementBy1:	return 2;
			case PredecrementBy1:	return 2;

			case PostincrementBy2:	return 3;
			case PredecrementBy2:	return 3;

			case Offset8bitFromPC:	return 1;
			case Offset16bitFromPC:	return 4;

			case Extended:			return 1;
		}
	}

private:
	uint8_t format_;
	uint16_t continuation_ = 0;
};

}
