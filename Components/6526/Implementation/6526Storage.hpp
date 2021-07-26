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

		template <int shift> void set_reload(uint8_t v) {
			reload = (reload & (0xff00 >> shift)) | uint16_t(v << shift);

			if constexpr (shift == 8) {
				if(!(control&1)) {
					value = reload;

					if(control&8) {
						control |= 1;	// At a guess: start one-shot automatically (?)
					}
				}
			}
		}

		template <bool is_counter_2> void set_control(uint8_t v) {
			control = v & 0xef;
			if(v & 0x10) {
				value = reload;
			}

			// Force reload + one-shot => start counting (?)
			if((v & 0x18) == 0x18) {
				control |= 1;
			}
		}

		int subtract(int count) {
			if(control & 8) {
				// One-shot.
				if(value < count) {
					value = reload;
					control &= 0xfe;
					return 1;
				} else {
					value -= count;
				}
				return 0;
			} else {
				// Continuous.
				value -= count;

				value -= (reload + 1);
				const int underflows = -value / (reload + 1);
				value %= (reload + 1);
				value += (reload + 1);

				return underflows;
			}
		}
	} counter_[2];
};

}
}

#endif /* _526Storage_h */
