//
//  MemorySlotHandler.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 03/01/2018.
//  Copyright 2018 Thomas Harte. All rights reserved.
//

#ifndef MemorySlotHandler_hpp
#define MemorySlotHandler_hpp

#include "../../ClockReceiver/ClockReceiver.hpp"
#include "../../Analyser/Dynamic/ConfidenceCounter.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

/*
	Design assumptions:

		- to-ROM writes and paging events are 'rare', so virtual call costs aren't worrisome;
		- ROM type variety is sufficiently slender that most of it can be built into the MSX.

	Part of the motivation is also that the MSX has four logical slots, the ROM, RAM plus two
	things plugged in. So even if the base class were templated to remove the virtual call,
	there'd just be a switch on what to call.
*/
namespace MSX {

class MemorySlot {
	public:
		MemorySlot();

		/// Attempts to write the argument as the secondary paging selection.
		void set_secondary_paging(uint8_t);

		/// @returns The value most recently provided to @c set_secondary_paging.
		uint8_t secondary_paging() const;

		/// Indicates whether this slot supports secondary paging.
		bool supports_secondary_paging = false;

		/// @returns A pointer to the area of memory currently underneath @c address that
		/// should be read
		const uint8_t *read_pointer(int segment) const;

		/// @returns A pointer to the area of memory currently underneath @c address.
		uint8_t *write_pointer(int segment) const;

		/// Sets the value most-recently written to one of the standard
		/// memory mapping ports, FC–FF.
		void apply_mapping(uint8_t port, uint8_t value);

		/// Copies an underlying source buffer.
		void set_source(const std::vector<uint8_t> &source);

		/// Provides a reference to the internal source storage.
		const std::vector<uint8_t> &source() const;

		/// Maps the content from @c source_address in the buffer previously
		/// supplied to @c set_source to the region indicated by
		/// @c destination_address and @c length within @c subslot.
		void map(
			int subslot,
			std::size_t source_address,
			uint16_t destination_address,
			std::size_t length);

		/// Marks the region indicated by @c destination_address and @c length
		/// as unmapped. In practical terms that means that a @c MemorySlotHandler
		/// will be used to field accesses to that area, allowing for areas that are not
		/// backed by memory to be modelled.
		void unmap(
			int subslot,
			uint16_t destination_address,
			std::size_t length);

	private:
		std::vector<uint8_t> source_;
		uint8_t *read_pointers_[4][8];
		uint8_t *write_pointers_[4][8];
		uint8_t secondary_paging_ = 0;

		using MemoryChunk = std::array<uint8_t, 8192>;
		inline static MemoryChunk unmapped{0xff};
		inline static MemoryChunk scratch;
};

class MemorySlotHandler {
	public:
		virtual ~MemorySlotHandler() {}

		/*! Advances time by @c half_cycles. */
		virtual void run_for([[maybe_unused]] HalfCycles half_cycles) {}

		/*! Announces an attempt to write @c value to @c address. */
		virtual void write(uint16_t address, uint8_t value, bool pc_is_outside_bios) = 0;

		/*! Seeks the result of a read at @c address; this is used only if the area is unmapped. */
		virtual uint8_t read([[maybe_unused]] uint16_t address) { return 0xff; }

		/*! @returns The probability that this handler is correct for the data it owns. */
		float get_confidence() {
			return confidence_counter_.get_confidence();
		}

		virtual std::string debug_type() {
			return "";
		}

	protected:
		Analyser::Dynamic::ConfidenceCounter confidence_counter_;
};

}

#endif /* MemorySlotHandler_hpp */
