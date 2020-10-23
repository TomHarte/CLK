//
//  LanguageCardSwitches.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 22/10/2020.
//  Copyright © 2020 Thomas Harte. All rights reserved.
//

#ifndef LanguageCardSwitches_h
#define LanguageCardSwitches_h

namespace Apple {
namespace II {

template <typename Machine> class LanguageCardSwitches {
	public:
		struct State {
			/// Indicates which 4kb chunk of RAM should be visible at $Dxxx if RAM is visible at all.
			bool bank1 = false;

			/// @c true indicates that RAM should be readable in the range $D000–$FFFF;
			/// @c indicates ROM should be readable.
			bool read = false;

			/// @c true indicates that ROM is selected for 'writing' in the range $D000–$FFFF (i.e. writes are a no-op);
			/// @c false indicates that RAM is selected for writing.
			bool write = false;

			/// Contains the state of the internal pre-write flip flop; it does not directly affect the current memory map.
			bool pre_write = false;
		};

		LanguageCardSwitches(Machine &machine) : machine_(machine) {}

		/// Used by an owner to forward any access to $c08x.
		void access(uint16_t address, bool is_read) {
			// Quotes below taken from Understanding the Apple II, p. 5-28 and 5-29.

			// "A3 controls the 4K bank selection"
			state_.bank1 = address & 8;

			// "Access to $C080, $C083, $C084, $0087, $C088, $C08B, $C08C, or $C08F sets the READ ENABLE flip-flop"
			// (other accesses reset it)
			state_.read = !(((address&2) >> 1) ^ (address&1));

			// "The WRITE ENABLE' flip-flop is reset by an odd read access to the $C08X range when the PRE-WRITE flip-flop is set."
			if(state_.pre_write && is_read && (address&1)) state_.write = false;

			// "[The WRITE ENABLE' flip-flop] is set by an even access in the $C08X range."
			if(!(address&1)) state_.write = true;

			// ("Any other type of access causes the WRITE ENABLE' flip-flop to hold its current state.")

			// "The PRE-WRITE flip-flop is set by an odd read access in the $C08X range. It is reset by an even access or a write access."
			state_.pre_write = is_read ? (address&1) : false;

			// Apply whatever the net effect of all that is to the memory map.
			machine_.set_language_card_paging();
		}

		/// Provides read-only access to the current language card switch state.
		const State &state() {
			return state_;
		}

	private:
		Machine &machine_;
		State state_;
};

}
}

#endif /* LanguageCard_h */
