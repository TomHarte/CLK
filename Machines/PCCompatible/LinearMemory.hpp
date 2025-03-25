//
//  LinearMemory.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 24/03/2025.
//  Copyright © 2025 Thomas Harte. All rights reserved.
//

#pragma once

#include "InstructionSets/x86/AccessType.hpp"
#include "InstructionSets/x86/Model.hpp"
#include "Machines/Utility/MemoryFuzzer.hpp"

#include <algorithm>
#include <array>
#include <cassert>
#include <cstring>
#include <cstdint>
#include <utility>

namespace PCCompatible {

// TODO: send writes to the ROM area off to nowhere.
// TODO: support banked sections for EGA/VGA and possibly EMS purposes.

template <size_t MaxAddressV>
struct LinearPool {
	static constexpr size_t MaxAddress = MaxAddressV;

	LinearPool() {
		Memory::Fuzz(memory);
	}

	//
	// External access.
	//

	// Provided for setup.
	void install(const uint32_t address, const uint8_t *const data, const uint32_t length) {
		std::copy(data, data + length, memory.begin() + std::vector<uint8_t>::difference_type(address));
	}

	// Used by both DMA devices and by the CGA and MDA cards to set up their base pointers.
	// @c address is always physical.
	uint8_t *at(const uint32_t address) {
		return &memory[address];
	}

	template <typename IntT>
	IntT read(const uint32_t address) {
		return *reinterpret_cast<IntT *>(&memory[address]);
	}

protected:
	std::array<uint8_t, MaxAddress> memory;
};

struct SplitHolder {
	using AccessType = InstructionSet::x86::AccessType;

	template <typename IntT, AccessType type>
	typename InstructionSet::x86::Accessor<IntT, type>::type access(
		const uint32_t address,
		const uint32_t base,
		const uint32_t bytes_available,
		uint8_t *memory
	) {
		if(bytes_available >= sizeof(IntT)) {
			return *reinterpret_cast<IntT *>(&memory[address]);
		}

		// This is a large quantity that straddles the limit,
		// but if it's being read only then just assemble it and
		// forget about things...
		if constexpr (!is_writeable(type)) {
			if constexpr (std::is_same_v<IntT, uint16_t>) {
				return uint16_t(memory[address] | (memory[base] << 8));
			}

			IntT result;
			auto buffer = reinterpret_cast<uint8_t *>(&result);
			std::memcpy(buffer, &memory[address], bytes_available);
			std::memcpy(buffer + bytes_available, &memory[base], sizeof(IntT) - bytes_available);
			return result;
		}

		// The caller needs an atomic unit that looks like an IntT and will
		// need to be written out eventually, so set up for that.
		write_back_address_[0] = address;
		write_back_address_[1] = base;
		write_back_lead_size_ = bytes_available;

		// Seed value only if this is a modify
		if constexpr (type == AccessType::ReadModifyWrite) {
			auto buffer = reinterpret_cast<uint8_t *>(&write_back_value_);
			if constexpr (std::is_same_v<IntT, uint16_t>) {
				buffer[0] = memory[address];
				buffer[1] = memory[base];
			} else {
				std::memcpy(buffer, &memory[address], bytes_available);
				std::memcpy(buffer + bytes_available, &memory[base], sizeof(IntT) - bytes_available);
			}
		}

		return *reinterpret_cast<IntT *>(&write_back_value_);
	}

	template <typename IntT>
	void write_back(uint8_t *memory) {
		if constexpr (std::is_same_v<IntT, uint8_t>) {
			return;
		}

		if(write_back_address_[0] == NoWriteBack) {
			return;
		}

		auto buffer = reinterpret_cast<uint8_t *>(&write_back_value_);
		if constexpr (std::is_same_v<IntT, uint16_t>) {
			memory[write_back_address_[0]] = buffer[0];
			memory[write_back_address_[1]] = buffer[1];
		} else {
			std::memcpy(&memory[write_back_address_[0]], buffer, write_back_lead_size_);
			std::memcpy(&memory[write_back_address_[1]], buffer + write_back_lead_size_, sizeof(IntT) - write_back_lead_size_);
		}
		write_back_address_[0] = NoWriteBack;
	}

private:
	static constexpr uint32_t NoWriteBack = 0;	// A low byte address of 0 can't require write-back.
	uint32_t write_back_address_[2] = {NoWriteBack, NoWriteBack};
	uint32_t write_back_lead_size_;
	uint32_t write_back_value_;
};


template <InstructionSet::x86::Model model, typename Enable = void> struct LinearMemory;

template <InstructionSet::x86::Model model>
struct LinearMemory<model, std::enable_if_t<model <= InstructionSet::x86::Model::i80186>>: public SplitHolder, public LinearPool<1 << 20> {
	template <typename IntT, AccessType type>
	typename InstructionSet::x86::Accessor<IntT, type>::type access(
		uint32_t address,
		const uint32_t base
	) {
		address &= MaxAddress - 1;

		// Bytes: always safe.
		if constexpr (std::is_same_v<IntT, uint8_t>) {
			return memory[address];
		}

		// Split on end of address space.
		if(address == MaxAddress - 1) {
			return SplitHolder::access<IntT, type>(address, base, 1, memory.data());
		}

		// Split on end of segment if this is an 8086.
		if constexpr (model == InstructionSet::x86::Model::i8086) {
			const uint32_t offset = address - base;
			if(offset == 0xffff) {
				return SplitHolder::access<IntT, type>(address, base, 1, memory.data());
			}
		}

		// Don't split.
		return *reinterpret_cast<IntT *>(&memory[address]);
	}

	template <typename IntT>
	void write_back() {
		SplitHolder::write_back<IntT>(memory.data());
	}

	template <typename IntT>
	void preauthorised_write(
		uint32_t address,
		const uint32_t base,
		IntT value
	) {
		address &= MaxAddress - 1;

		// Bytes can be written without further ado.
		if constexpr (std::is_same_v<IntT, uint8_t>) {
			memory[address] = value;
			return;
		}

		// Words that straddle the segment end must be split in two.
		if constexpr (model == InstructionSet::x86::Model::i8086) {
			const uint32_t offset = address - base;
			if(offset == 0xffff) {
				memory[address] = uint8_t(value & 0xff);
				memory[base] = uint8_t(value >> 8);
				return;
			}
		}

		// Words that straddle the end of physical RAM must also be split in two.
		if(address == MaxAddress - 1) {
			memory[address] = uint8_t(value & 0xff);
			memory[0] = uint8_t(value >> 8);
			return;
		}

		// It's safe just to write then.
		*reinterpret_cast<IntT *>(&memory[address]) = value;
	}
};

template <>
struct LinearMemory<InstructionSet::x86::Model::i80286>: public LinearPool<1 << 24> {
	// A20 is the only thing that can cause split accesses on an 80286.
	void set_a20_enabled(const bool enabled) {
		address_mask_ = uint32_t(~0);
		if(!enabled) {
			address_mask_ &= uint32_t(~(1 << 20));
		}
	}

	using AccessType = InstructionSet::x86::AccessType;
	template <typename IntT, AccessType type>
	typename InstructionSet::x86::Accessor<IntT, type>::type access(
		uint32_t address, uint32_t
	) {
		// 80286: never split (probably?).
		return *reinterpret_cast<IntT *>(&memory[address & address_mask_]);
	}

	template <typename IntT>
	void write_back() {}

	template <typename IntT>
	void preauthorised_write(
		uint32_t address,
		const uint32_t,
		IntT value
	) {
		*reinterpret_cast<IntT *>(&memory[address & address_mask_]) = value;
	}

private:
	uint32_t address_mask_;
};

}
