//
//  6526Storage.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 18/07/2021.
//  Copyright © 2021 Thomas Harte. All rights reserved.
//

#ifndef _526Storage_h
#define _526Storage_h

namespace MOS {
namespace MOS6526 {

template <bool is_8250> struct TODStorage {};
template <> struct TODStorage<false> {
	// TODO.
};
template <> struct TODStorage<true> {
	uint32_t increment_mask = uint32_t(~0);
	uint32_t latch = 0;
	uint32_t value = 0;
	uint32_t alarm = 0;
};

struct MOS6526Storage {
	HalfCycles half_divider_;

	uint8_t output_[2] = {0, 0};
	uint8_t data_direction_[2] = {0, 0};

	uint8_t interrupt_control_ = 0;
	uint8_t interrupt_state_ = 0;

	struct Counter {
		uint16_t reload = 0;
		uint16_t value = 0;
		uint8_t control = 0;

		template <int shift> void set_reload(uint8_t v) {
			reload = (reload & (0xff00 >> shift)) | uint16_t(v << shift);

			if constexpr (shift == 8) {
				if(!(control&1)) {
					pending |= ReloadInOne;
				}
			}

			// If this write has hit during a reload cycle, reload.
			if(pending & ReloadNow) {
				value = reload;
			}
		}

		template <bool is_counter_2> void set_control(uint8_t v) {
			control = v;
		}

		template <bool is_counter_2> bool advance(bool chained_input) {
			// TODO: remove most of the conditionals here in favour of bit shuffling.

			pending = (pending & PendingClearMask) << 1;

			//
			// Apply feeder states inputs: anything that
			// will take effect in the future.
			//

			// Schedule a force reload if requested.
			if(control & 0x10) {
				pending |= ReloadInOne;
				control &= ~0x10;
			}

			// Determine whether an input clock is applicable.
			if constexpr(is_counter_2) {
				switch(control&0x60) {
					case 0x00:	// Count Phi2 pulses.
						pending |= TestInputNow;
					break;
					case 0x40:	// Count timer A reloads.
						pending |= chained_input ? TestInputNow : 0;
					break;

					case 0x20:	// Count negative CNTs.
					case 0x60:	// Count timer A transitions when CNT is low.
					default:
						// TODO: all other forms of input.
						assert(false);
					break;
				}
			} else {
				if(!(control&0x20)) {
					pending |= TestInputNow;
				} else if (chained_input) {	// TODO: check CNT directly, probably?
					pending |= TestInputInOne;
				}
			}
			if(pending&TestInputNow && control&1) {
				pending |= ApplyClockInTwo;
			}

			// Keep a history of the one-shot bit.
			if(control & 0x08) {
				pending |= OneShotInOne | OneShotNow;
			}


			//
			// Perform a timer tick and decide whether a reload is prompted.
			//
			if(pending & ApplyClockNow) {
				--value;
			}

			const bool should_reload = !value && (pending & ApplyClockInOne);

			// Schedule a reload if so ordered.
			if(should_reload) {
				pending |= ReloadNow;	// Combine this decision with a deferred
										// input from the force-reoad test above.

				// If this was one-shot, stop.
				if(pending&(OneShotInOne | OneShotNow)) {
					control &= ~1;
					pending &= ~(ApplyClockInOne|ApplyClockInTwo);	// Cancel scheculed ticks.
				}
			}

			// Reload if scheduled.
			if(pending & ReloadNow) {
				value = reload;
				pending &= ~ApplyClockInOne;	// Skip next decrement.
			}


			return should_reload;
		}

		private:
			int pending = 0;

			static constexpr int ReloadInOne = 1 << 0;
			static constexpr int ReloadNow = 1 << 1;

			static constexpr int OneShotInOne = 1 << 2;
			static constexpr int OneShotNow = 1 << 3;

			static constexpr int ApplyClockInTwo = 1 << 4;
			static constexpr int ApplyClockInOne = 1 << 5;
			static constexpr int ApplyClockNow = 1 << 6;

			static constexpr int TestInputInOne = 1 << 7;
			static constexpr int TestInputNow = 1 << 8;

			static constexpr int PendingClearMask = ~(ReloadNow | OneShotNow | ApplyClockNow);

			bool active_ = false;
	} counter_[2];

	static constexpr int InterruptInOne = 1 << 0;
	static constexpr int InterruptNow = 1 << 1;
	static constexpr int PendingClearMask = ~(InterruptNow);
	int pending_ = 0;
};

}
}

#endif /* _526Storage_h */
