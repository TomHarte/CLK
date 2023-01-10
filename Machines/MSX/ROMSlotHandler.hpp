//
//  ROMSlotHandler.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 03/01/2018.
//  Copyright 2018 Thomas Harte. All rights reserved.
//

#ifndef ROMSlotHandler_hpp
#define ROMSlotHandler_hpp

#include "../../ClockReceiver/ClockReceiver.hpp"
#include "../../Analyser/Dynamic/ConfidenceCounter.hpp"

#include <cstddef>
#include <cstdint>
#include <string>

/*
	Design assumptions:

		- to-ROM writes and paging events are 'rare', so virtual call costs aren't worrisome;
		- ROM type variety is sufficiently slender that most of it can be built into the MSX.

	Part of the motivation is also that the MSX has four logical slots, the ROM, RAM plus two
	things plugged in. So even if the base class were templated to remove the virtual call,
	there'd just be a switch on what to call.
*/
namespace MSX {

class MemoryMap {
	public:
		/*!
			Maps source data from the ROM's source to the given address range.
		*/
		virtual void map(int slot, std::size_t source_address, uint16_t destination_address, std::size_t length) = 0;

		/*!
			Unmaps source data from the given address range; the slot handler's read function will be used
			to respond to queries in that range.
		*/
		virtual void unmap(int slot, uint16_t destination_address, std::size_t length) = 0;
};

class ROMSlotHandler {
	public:
		virtual ~ROMSlotHandler() {}

		/*! Advances time by @c half_cycles. */
		virtual void run_for([[maybe_unused]] HalfCycles half_cycles) {}

		/*! Announces an attempt to write @c value to @c address. */
		virtual void write(uint16_t address, uint8_t value, bool pc_is_outside_bios) = 0;

		/*! Seeks the result of a read at @c address; this is used only if the area is unmapped. */
		virtual uint8_t read([[maybe_unused]] uint16_t address) { return 0xff; }

		enum class WrappingStrategy {
			/// All accesses are modulo the size of the ROM.
			Repeat,
			/// Out-of-bounds accesses read a vacant bus.
			Empty
		};

		/*! @returns The wrapping strategy to apply to mapping requests from this ROM slot. */
		virtual WrappingStrategy wrapping_strategy() const {
			return WrappingStrategy::Repeat;
		}

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

#endif /* ROMSlotHandler_hpp */
