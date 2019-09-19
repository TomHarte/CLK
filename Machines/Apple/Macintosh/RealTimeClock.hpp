//
//  RealTimeClock.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 07/05/2019.
//  Copyright © 2019 Thomas Harte. All rights reserved.
//

#ifndef RealTimeClock_hpp
#define RealTimeClock_hpp

#include "../../Utility/MemoryFuzzer.hpp"

namespace Apple {
namespace Macintosh {

/*!
	Models the storage component of Apple's real-time clock.

	Since tracking of time is pushed to this class, it is assumed
	that whomever is translating real time into emulated time
	will notify the VIA of a potential interrupt.
*/
class RealTimeClock {
	public:
		RealTimeClock() {
			// TODO: this should persist, if possible, rather than
			// being default initialised.
			const uint8_t default_data[] = {
				0xa8, 0x00, 0x00, 0x00,
				0xcc, 0x0a, 0xcc, 0x0a,
				0x00, 0x00, 0x00, 0x00,
				0x00, 0x02, 0x63, 0x00,
				0x03, 0x88, 0x00, 0x4c
			};
			memcpy(data_, default_data, sizeof(data_));
		}

		/*!
			Advances the clock by 1 second.

			The caller should also notify the VIA.
		*/
		void update() {
			for(int c = 0; c < 4; ++c) {
				++seconds_[c];
				if(seconds_[c]) break;
			}
		}

		/*!
			Sets the current clock and data inputs to the clock.
		*/
		void set_input(bool clock, bool data) {
			/*
				Documented commands:

					z0000001		Seconds register 0 (lowest order byte)
					z0000101		Seconds register 1
					z0001001		Seconds register 2
					z0001101		Seconds register 3
					00110001		Test register (write only)
					00110101		Write-protect register (write only)
					z010aa01		RAM addresses 0x10 - 0x13
					z1aaaa01		RAM addresses 0x00 – 0x0f

					z = 1 => a read; z = 0 => a write.

				The top bit of the write-protect register enables (0) or disables (1)
				writes to other locations.

				All the documentation says about the test register is to set the top
				two bits to 0 for normal operation. Abnormal operation is undefined.

				The data line is valid when the clock transitions to level 0.
			*/

			if(clock && !previous_clock_) {
				// Shift into the command_ register, no matter what.
				command_ = uint16_t((command_ << 1) | (data ? 1 : 0));
				result_ <<= 1;

				// Increment phase.
				++phase_;

				// When phase hits 8, inspect the command.
				// If it's a read, prepare a result.
				if(phase_ == 8) {
					if(command_ & 0x80) {
						// A read.
						const auto address = (command_ >> 2) & 0x1f;

						// Begin pessimistically.
						result_ = 0xff;

						if(address < 4) {
							result_ = seconds_[address];
						} else if(address >= 0x10) {
							result_ = data_[address & 0xf];
						} else if(address >= 0x8 && address <= 0xb) {
							result_ = data_[0x10 + (address & 0x3)];
						}
					}
				}

				// If phase hits 16 and this was a read command,
				// just stop. If it was a write command, do the
				// actual write.
				if(phase_ == 16) {
					if(!(command_ & 0x8000)) {
						// A write.

						const auto address = (command_ >> 10) & 0x1f;
						const uint8_t value = uint8_t(command_ & 0xff);

						// First test: is this to the write-protect register?
						if(address == 0xd) {
							write_protect_ = value;
						}

						// No other writing is permitted if the write protect
						// register won't allow it.
						if(!(write_protect_ & 0x80)) {
							if(address < 4) {
								seconds_[address] = value;
							} else if(address >= 0x10) {
								data_[address & 0xf] = value;
							} else if(address >= 0x8 && address <= 0xb) {
								data_[0x10 + (address & 0x3)] = value;
							}
						}
					}

					// A phase of 16 always ends the command, so reset here.
					abort();
				}
			}

			previous_clock_ = clock;
		}

		/*!
			Reads the current data output level from the clock.
		*/
		bool get_data() {
			return !!(result_ & 0x80);
		}

		/*!
			Announces that a serial command has been aborted.
		*/
		void abort() {
			result_ = 0;
			phase_ = 0;
			command_ = 0;
		}

	private:
		uint8_t data_[0x14];
		uint8_t seconds_[4];
		uint8_t write_protect_;

		int phase_ = 0;
		uint16_t command_;
		uint8_t result_ = 0;

		bool previous_clock_ = false;
};

}
}

#endif /* RealTimeClock_hpp */
