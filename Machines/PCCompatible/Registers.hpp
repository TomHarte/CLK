//
//  Registers.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 01/12/2023.
//  Copyright © 2023 Thomas Harte. All rights reserved.
//

#pragma once

#include "InstructionSets/x86/Descriptors.hpp"
#include "InstructionSets/x86/MachineStatus.hpp"
#include "InstructionSets/x86/Model.hpp"
#include "Numeric/RegisterSizes.hpp"

namespace PCCompatible {

template <InstructionSet::x86::Model>
struct Registers;

template <>
struct Registers<InstructionSet::x86::Model::i8086> {
public:
	static constexpr bool is_32bit = false;

	uint8_t &al()	{	return ax_.halves.low;	}
	uint8_t &ah()	{	return ax_.halves.high;	}
	uint16_t &ax()	{	return ax_.full;		}

	CPU::RegisterPair16 &axp()	{	return ax_;	}

	uint8_t &cl()	{	return cx_.halves.low;	}
	uint8_t &ch()	{	return cx_.halves.high;	}
	uint16_t &cx()	{	return cx_.full;		}

	uint8_t &dl()	{	return dx_.halves.low;	}
	uint8_t &dh()	{	return dx_.halves.high;	}
	uint16_t &dx()	{	return dx_.full;		}

	uint8_t &bl()	{	return bx_.halves.low;	}
	uint8_t &bh()	{	return bx_.halves.high;	}
	uint16_t &bx()	{	return bx_.full;		}

	uint16_t &sp()	{	return sp_;				}
	uint16_t &bp()	{	return bp_;				}
	uint16_t &si()	{	return si_;				}
	uint16_t &di()	{	return di_;				}

	uint16_t &ip()		{	return ip_;			}
	uint16_t ip() const	{	return ip_;			}

	uint16_t &es()		{	return segments_[Source::ES];	}
	uint16_t &cs()		{	return segments_[Source::CS];	}
	uint16_t &ds()		{	return segments_[Source::DS];	}
	uint16_t &ss()		{	return segments_[Source::SS];	}
	uint16_t es() const	{	return segments_[Source::ES];	}
	uint16_t cs() const	{	return segments_[Source::CS];	}
	uint16_t ds() const	{	return segments_[Source::DS];	}
	uint16_t ss() const	{	return segments_[Source::SS];	}
	uint16_t segment(const InstructionSet::x86::Source segment) const {
		return segments_[segment];
	}

	void reset() {
		segments_[Source::CS] = 0xffff;
		ip_ = 0;
	}

private:
	using Source = InstructionSet::x86::Source;

	CPU::RegisterPair16 ax_;
	CPU::RegisterPair16 cx_;
	CPU::RegisterPair16 dx_;
	CPU::RegisterPair16 bx_;

	uint16_t sp_;
	uint16_t bp_;
	uint16_t si_;
	uint16_t di_;
	uint16_t ip_;
	InstructionSet::x86::SegmentRegisterSet<uint16_t> segments_;
};

template <>
struct Registers<InstructionSet::x86::Model::i80186>: public Registers<InstructionSet::x86::Model::i8086> {};

template <>
struct Registers<InstructionSet::x86::Model::i80286>: public Registers<InstructionSet::x86::Model::i80186> {
public:
	void reset() {
		Registers<InstructionSet::x86::Model::i80186>::reset();
		machine_status_ = 0;
	}

	uint16_t msw() const {	return machine_status_;	}
	void set_msw(const uint16_t msw) {
		machine_status_ =
			(machine_status_ & InstructionSet::x86::MachineStatus::ProtectedModeEnable) |
			msw;
	}

	using DescriptorTable = InstructionSet::x86::DescriptorTable;
	using DescriptorTablePointer = InstructionSet::x86::DescriptorTablePointer;

	template <DescriptorTable table>
	void set(const DescriptorTablePointer location) {
		static constexpr bool is_global = table == DescriptorTable::Global;
		static_assert(is_global || table == DescriptorTable::Interrupt);
		auto &target = is_global ? global_ : interrupt_;
		target = location;
	}

	template <DescriptorTable table>
	const DescriptorTablePointer &get() const {
		if constexpr (table == DescriptorTable::Global) {
			return global_;
		} else if constexpr (table == DescriptorTable::Interrupt) {
			return interrupt_;
		} else {
			static_assert(table == DescriptorTable::Local);
			return local_;
		}
	}

private:
	uint16_t machine_status_;
	DescriptorTablePointer global_, interrupt_, local_;
};

}
