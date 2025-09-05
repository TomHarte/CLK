//
//  Registers.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 01/12/2023.
//  Copyright © 2023 Thomas Harte. All rights reserved.
//

#pragma once

#include "Descriptors.hpp"
#include "MachineStatus.hpp"
#include "Model.hpp"

#include "Numeric/RegisterSizes.hpp"

#include <cassert>

namespace InstructionSet::x86 {

template <Model>
struct Registers;

template <>
struct Registers<Model::i8086> {
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
	uint16_t segment(const Source segment) const {
		return segments_[segment];
	}

	void reset() {
		segments_[Source::CS] = 0xffff;
		ip_ = 0;
	}

	auto operator <=> (const Registers &rhs) const = default;

private:
	CPU::RegisterPair16 ax_;
	CPU::RegisterPair16 cx_;
	CPU::RegisterPair16 dx_;
	CPU::RegisterPair16 bx_;

	uint16_t sp_;
	uint16_t bp_;
	uint16_t si_;
	uint16_t di_;
	uint16_t ip_;
	SegmentRegisterSet<uint16_t> segments_;
};

template <>
struct Registers<Model::i80186>: public Registers<Model::i8086> {};

template <>
struct Registers<Model::i80286>: public Registers<Model::i80186> {
public:
	void reset() {
		Registers<Model::i80186>::reset();
		machine_status_ = 0;
		interrupt_ = DescriptorTablePointer{
			.limit = 256 * 4,
			.base = 0
		};
	}

	uint16_t msw() const {	return machine_status_;	}
	void set_msw(const uint16_t msw) {
		machine_status_ =
			(machine_status_ & MachineStatus::ProtectedModeEnable) |
			msw;
	}

	uint16_t task_state() const { return task_state_; }
	void set_task_state(const uint16_t tsr) {
		task_state_ = tsr;
	}

	uint16_t ldtr() const {	return ldtr_;	}
	void set_ldtr(const uint16_t ldtr) {
		ldtr_ = ldtr;
	}

	int privilege_level() const {
		return 0;	// TODO.
	}
	void set_privilege_level(int) {
		// TODO.
	}

	template <DescriptorTable table>
	void set(const DescriptorTablePointer location) {
		switch(table) {
			case DescriptorTable::Local:		local_ = location; 		break;
			case DescriptorTable::Global:		global_ = location; 	break;
			case DescriptorTable::Interrupt:	interrupt_ = location;	break;
		}
	}

	template <DescriptorTable table>
	const DescriptorTablePointer &get() const {
		switch(table) {
			case DescriptorTable::Local:		return local_;
			case DescriptorTable::Global:		return global_;
			default:
				assert(table == DescriptorTable::Interrupt);
				return interrupt_;
		}
	}

private:
	uint16_t machine_status_;
	DescriptorTablePointer global_, interrupt_, local_;
	uint16_t ldtr_, task_state_;
};

}
