//
//  Memory.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 01/12/2023.
//  Copyright © 2023 Thomas Harte. All rights reserved.
//

#pragma once

#include "ProcessorByModel.hpp"
#include "Registers.hpp"
#include "Segments.hpp"

#include "Analyser/Static/PCCompatible/Target.hpp"
#include "InstructionSets/x86/AccessType.hpp"

#include <array>

namespace PCCompatible {

// TODO: send writes to the ROM area off to nowhere.
template <Analyser::Static::PCCompatible::Model model>
class Memory {
	static constexpr auto x86_model = processor_model(model);

public:
	using AccessType = InstructionSet::x86::AccessType;

	// Constructor.
	Memory(Registers<x86_model> &registers, const Segments<x86_model> &segments) :
		registers_(registers), segments_(segments) {}

	//
	// Preauthorisation call-ins. Since only an 8088 is currently modelled, all accesses are implicitly authorised.
	//
	void preauthorise_stack_write([[maybe_unused]] uint32_t length) {}
	void preauthorise_stack_read([[maybe_unused]] uint32_t length) {}
	void preauthorise_read([[maybe_unused]] InstructionSet::x86::Source segment, [[maybe_unused]] uint16_t start, [[maybe_unused]] uint32_t length) {}
	void preauthorise_read([[maybe_unused]] uint32_t start, [[maybe_unused]] uint32_t length) {}

	//
	// Access call-ins.
	//

	// Accesses an address based on segment:offset.
	template <typename IntT, AccessType type>
	typename InstructionSet::x86::Accessor<IntT, type>::type access(
		const InstructionSet::x86::Source segment,
		const uint16_t offset
	) {
		const uint32_t physical_address = address(segment, offset);

		if constexpr (std::is_same_v<IntT, uint16_t>) {
			// If this is a 16-bit access that runs past the end of the segment, it'll wrap back
			// to the start. So the 16-bit value will need to be a local cache.
			if(offset == 0xffff) {
				return split_word<type>(physical_address, address(segment, 0));
			}
		}

		return access<IntT, type>(physical_address);
	}

	// Accesses an address based on physical location.
	template <typename IntT, AccessType type>
	typename InstructionSet::x86::Accessor<IntT, type>::type access(const uint32_t address) {
		// Dispense with the single-byte case trivially.
		if constexpr (std::is_same_v<IntT, uint8_t>) {
			return memory[address];
		} else if(address != 0xf'ffff) {
			return *reinterpret_cast<IntT *>(&memory[address]);
		} else {
			return split_word<type>(address, 0);
		}
	}

	template <typename IntT>
	void write_back() {
		if constexpr (std::is_same_v<IntT, uint16_t>) {
			if(write_back_address_[0] != NoWriteBack) {
				memory[write_back_address_[0]] = write_back_value_ & 0xff;
				memory[write_back_address_[1]] = write_back_value_ >> 8;
				write_back_address_[0]  = 0;
			}
		}
	}

	//
	// Direct read and write.
	//
	template <typename IntT>
	void preauthorised_write(
		const InstructionSet::x86::Source segment,
		const uint16_t offset,
		const IntT value
	) {
		// Bytes can be written without further ado.
		if constexpr (std::is_same_v<IntT, uint8_t>) {
			memory[address(segment, offset) & 0xf'ffff] = value;
			return;
		}

		// Words that straddle the segment end must be split in two.
		if(offset == 0xffff) {
			memory[address(segment, offset) & 0xf'ffff] = value & 0xff;
			memory[address(segment, 0x0000) & 0xf'ffff] = value >> 8;
			return;
		}

		const uint32_t target = address(segment, offset) & 0xf'ffff;

		// Words that straddle the end of physical RAM must also be split in two.
		if(target == 0xf'ffff) {
			memory[0xf'ffff] = value & 0xff;
			memory[0x0'0000] = value >> 8;
			return;
		}

		// It's safe just to write then.
		*reinterpret_cast<IntT *>(&memory[target]) = value;
	}

	template <typename IntT>
	IntT preauthorised_read(
		const InstructionSet::x86::Source segment,
		const uint16_t offset
	) {
		// Bytes can be written without further ado.
		if constexpr (std::is_same_v<IntT, uint8_t>) {
			return memory[address(segment, offset) & 0xf'ffff];
		}

		// Words that straddle the segment end must be split in two.
		if(offset == 0xffff) {
			return IntT(
				memory[address(segment, offset) & 0xf'ffff] |
				memory[address(segment, 0x0000) & 0xf'ffff] << 8
			);
		}

		const uint32_t target = address(segment, offset) & 0xf'ffff;

		// Words that straddle the end of physical RAM must also be split in two.
		if(target == 0xf'ffff) {
			return IntT(
				memory[0xf'ffff] |
				memory[0x0'0000] << 8
			);
		}

		// It's safe just to write then.
		return *reinterpret_cast<IntT *>(&memory[target]);
	}

	//
	// Helper for instruction fetch.
	//
	std::pair<const uint8_t *, size_t> next_code() const {
		const uint32_t start = segments_.cs_base_ + registers_.ip();
		return std::make_pair(&memory[start], 0x10'000 - start);
	}

	std::pair<const uint8_t *, size_t> all() const {
		return std::make_pair(memory.data(), 0x10'000);
	}

	//
	// External access.
	//
	void install(size_t address, const uint8_t *data, size_t length) {
		std::copy(data, data + length, memory.begin() + std::vector<uint8_t>::difference_type(address));
	}

	uint8_t *at(uint32_t address) {
		return &memory[address];
	}

private:
	std::array<uint8_t, 1024*1024> memory{0xff};
	Registers<x86_model> &registers_;
	const Segments<x86_model> &segments_;

	uint32_t segment_base(const InstructionSet::x86::Source segment) const {
		using Source = InstructionSet::x86::Source;
		switch(segment) {
			default:			return segments_.ds_base_;
			case Source::ES:	return segments_.es_base_;
			case Source::CS:	return segments_.cs_base_;
			case Source::SS:	return segments_.ss_base_;
		}
	}

	uint32_t address(const InstructionSet::x86::Source segment, const uint16_t offset) const {
		return (segment_base(segment) + offset) & 0xf'ffff;
	}

	template <AccessType type>
	typename InstructionSet::x86::Accessor<uint16_t, type>::type
	split_word(const uint32_t low_address, const uint32_t high_address) {
		if constexpr (is_writeable(type)) {
			write_back_address_[0] = low_address;
			write_back_address_[1] = high_address;

			// Prepopulate only if this is a modify.
			if constexpr (type == AccessType::ReadModifyWrite) {
				write_back_value_ = uint16_t(memory[write_back_address_[0]] | (memory[write_back_address_[1]] << 8));
			}

			return write_back_value_;
		} else {
			return uint16_t(memory[low_address] | (memory[high_address] << 8));
		}
	}

	static constexpr uint32_t NoWriteBack = 0;	// A low byte address of 0 can't require write-back.
	uint32_t write_back_address_[2] = {NoWriteBack, NoWriteBack};
	uint16_t write_back_value_;
};

}
