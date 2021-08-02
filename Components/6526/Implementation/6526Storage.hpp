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

struct MOS6526Storage {
	HalfCycles half_divider_;

	uint8_t output_[2] = {0, 0};
	uint8_t data_direction_[2] = {0, 0};

	uint8_t interrupt_control_ = 0;
	uint8_t interrupt_state_ = 0;

	uint32_t tod_increment_mask_ = uint32_t(~0);
	uint32_t tod_latch_ = 0;
	uint32_t tod_ = 0;
	uint32_t tod_alarm_ = 0;

	struct Counter {
		uint16_t reload = 0;
		uint16_t value = 0;
		uint8_t control = 0;
		bool hit_zero = false;

		template <int shift> void set_reload(uint8_t v) {
			reload = (reload & (0xff00 >> shift)) | uint16_t(v << shift);

			if constexpr (shift == 8) {
				if(!(control&1)) {
					pending |= ReloadInOne;
				}
			}
		}

		template <bool is_counter_2> void set_control(uint8_t v) {
			control = v;
		}

		void advance(bool chained_input) {
			// TODO: remove most of the conditionals here.

			pending <<= 1;

			if(control & 0x10) {
				pending |= ReloadInOne;
				control &= ~0x10;
			}

			if((control & 0x01) || chained_input) {
				pending |= ApplyClockInTwo;
			}
			if(control & 0x08) {
				pending |= OneShotInOne;
			}

			if((pending & ReloadNow) || (hit_zero && (pending & ApplyClockInTwo))) {
				value = reload;
				pending &= ~ApplyClockNow;	// Skip one decrement.
			}

			if(pending & ApplyClockNow) {
				--value;
				hit_zero = !value;
			} else {
				hit_zero = false;
			}

			if(hit_zero && pending&(OneShotInOne | OneShotNow)) {
				control &= ~1;
			}

			// Clear any bits that would flow into the wrong field.
			pending &= PendingClearMask;
		}

		private:
			int pending = 0;

//			static constexpr int ReloadInThree = 1 << 0;
//			static constexpr int ReloadInTwo = 1 << 1;
			static constexpr int ReloadInOne = 1 << 2;
			static constexpr int ReloadNow = 1 << 3;

//			static constexpr int OneShotInTwo = 1 << 4;
			static constexpr int OneShotInOne = 1 << 5;
			static constexpr int OneShotNow = 1 << 6;

			static constexpr int ApplyClockInThree = 1 << 7;
			static constexpr int ApplyClockInTwo = 1 << 8;
			static constexpr int ApplyClockInOne = 1 << 9;
			static constexpr int ApplyClockNow = 1 << 10;

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
