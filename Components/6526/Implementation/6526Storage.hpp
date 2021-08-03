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

class TODBase {
	public:
		template <bool is_timer2> void set_control(uint8_t value) {
			if constexpr (is_timer2) {
				write_alarm = value & 0x80;
			} else {
				is_50Hz = value & 0x80;
			}
		}

	protected:
		bool write_alarm = false, is_50Hz = false;
};

template <bool is_8250> class TODStorage {};
template <> class TODStorage<false>: public TODBase {
	public:
		// TODO.

		template <int byte> void write(uint8_t) {
			printf("6526 TOD clock not implemented\n");
			assert(false);
		}

		template <int byte> uint8_t read() {
			printf("6526 TOD clock not implemented\n");
			assert(false);
		}

		bool advance(int) {
			printf("6526 TOD clock not implemented\n");
			assert(false);
		}
};
template <> class TODStorage<true>: public TODBase {
	private:
		uint32_t increment_mask_ = uint32_t(~0);
		uint32_t latch_ = 0;
		uint32_t value_ = 0;
		uint32_t alarm_ = 0;

	public:
		template <int byte> void write(uint8_t v) {
			if constexpr (byte == 3) {
				return;
			}
			constexpr int shift = byte << 3;

			// Write to either the alarm or the current value as directed;
			// writing to any part of the current value other than the LSB
			// pauses incrementing until the LSB is written.
			if(write_alarm) {
				alarm_ = (alarm_ & (0x00ff'ffff >> (24 - shift))) | uint32_t(v << shift);
			} else {
				value_ = (alarm_ & (0x00ff'ffff >> (24 - shift))) | uint32_t(v << shift);
				increment_mask_ = (shift == 0) ? uint32_t(~0) : 0;
			}
		}

		template <int byte> uint8_t read() {
			if constexpr (byte == 3) {
				return 0xff;	// Assumed. Just a guss.
			}
			constexpr int shift = byte << 3;

			if(latch_) {
				// Latching: if this is a latched read from the LSB,
				// empty the latch.
				const uint8_t result = uint8_t((latch_ >> shift) & 0xff);
				if constexpr (shift == 0) {
					latch_ = 0;
				}
				return result;
			} else {
				// Latching: if this is a read from the MSB, latch now.
				if constexpr (shift == 16) {
					latch_ = value_ | 0xff00'0000;
				}
				return uint8_t((value_ >> shift) & 0xff);
			}
		}

		bool advance(int count) {
			// The 8250 uses a simple binary counter to replace the
			// 6526's time-of-day clock. So this is easy.
			const uint32_t distance_to_alarm = (alarm_ - value_) & 0xffffff;
			value_ += uint32_t(count) & increment_mask_;
			return distance_to_alarm <= uint32_t(count);
		}
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
