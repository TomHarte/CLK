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

#include <array>
#include <cassert>
#include <cstring>
#include <cstdint>

namespace PCCompatible {

/*!
	Provides a mapping from linear addresses to underlying memory.

	Prior to the 80286, linear addresses are presently physical addresses.
	Some nuance might appear here if/when EGA and VGA and/or EMS are implemented.

	On an 8086 and 80186, addresses are clamped to 20 bits.

	On the 80286 they're clamped to 24 bits.

	From the AT onwards, address line 20 can be enabled or disabled.

	TODO: from the 80386 onwards, memory can be reordered and
	exceptions might be raised.

	TODO: remove assumption of a little-endian host.

	TODO: allow for read-only areas of memory, paged areas of memory, etc.
*/
template <InstructionSet::x86::Model model>
struct LinearMemory {
	using AccessType = InstructionSet::x86::AccessType;
	static constexpr uint32_t MaxAddress =
		1024 * 1024 * (model >= InstructionSet::x86::Model::i80286 ? 16 : 1);

	/// @returns A suitable accessor for the @c IntT that starts at @c address, subject to address
	/// wrapping within the bounds `[base, limit]`. There is a chance that some sort of behind-the-scenes
	/// activity may be necessary to present something as a single @c IntT that is actually split across the
	///
	template <typename IntT, AccessType type>
	typename InstructionSet::x86::Accessor<IntT, type>::type access(
		const uint32_t address,	/// Address within linear memory.
		uint32_t base,			/// Start address of wrapping window within linear memory.
		const uint32_t limit	/// Size of wrapping window.
	) {
		static_assert(
			std::is_same_v<IntT, uint8_t> ||
			std::is_same_v<IntT, uint16_t> ||
			std::is_same_v<IntT, uint32_t> ||
			!is_writeable(type)
		);
		assert(limit >= sizeof(IntT));

		base &= MaxAddress - 1;

		// Bytes: always safe.
		if constexpr (std::is_same_v<IntT, uint8_t>) {
			return memory[address];
		}

		// Larger quantities: split only if they won't fit.
		const auto bytes_available = base + limit - address;
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
	void write_back() {
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
	std::array<uint8_t, MaxAddress> memory{0xff};

	static constexpr uint32_t NoWriteBack = 0;	// A low byte address of 0 can't require write-back.
	uint32_t write_back_address_[2] = {NoWriteBack, NoWriteBack};
	uint32_t write_back_lead_size_;
	uint32_t write_back_value_;
};

}
