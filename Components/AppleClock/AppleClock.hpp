//
//  RealTimeClock.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 07/05/2019.
//  Copyright © 2019 Thomas Harte. All rights reserved.
//

#ifndef Apple_RealTimeClock_hpp
#define Apple_RealTimeClock_hpp

namespace Apple {
namespace Clock {

/*!
	Models Apple's real-time clocks, as contained in the Macintosh and IIgs.

	Since tracking of time is pushed to this class, it is assumed
	that whomever is translating real time into emulated time
	will also signal interrupts — this is just the storage and time counting.
*/
class ClockStorage {
	public:
		ClockStorage() {
			// TODO: this should persist, if possible, rather than
			// being default initialised.
			constexpr uint8_t default_data[] = {
				0xa8, 0x00, 0x00, 0x00,
				0xcc, 0x0a, 0xcc, 0x0a,
				0x00, 0x00, 0x00, 0x00,
				0x00, 0x02, 0x63, 0x00,
				0x03, 0x88, 0x00, 0x4c
			};
			memcpy(data_, default_data, sizeof(data_));
			memset(&data_[sizeof(default_data)], 0xff, sizeof(data_) - sizeof(default_data));
		}

		/*!
			Advances the clock by 1 second.

			The caller should also signal an interrupt.
		*/
		void update() {
			for(int c = 0; c < 4; ++c) {
				++seconds_[c];
				if(seconds_[c]) break;
			}
		}

	protected:
		static constexpr uint16_t NoResult = 0x100;
		static constexpr uint16_t DidWrite = 0x101;
		uint16_t perform(uint8_t command) {
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

					z0111abc, followed by 0defgh00
									RAM address abcdefgh

					z = 1 => a read; z = 0 => a write.

				The top bit of the write-protect register enables (0) or disables (1)
				writes to other locations.

				All the documentation says about the test register is to set the top
				two bits to 0 for normal operation. Abnormal operation is undefined.
			*/

			if(!is_writing_) {
				// Decode an address; use values >= 0x100 to represent clock time.
				address_ = (command >> 2) & 0x1f;
				if(address_ < 4) {
					address_ |= 0x100;
				} else if(address_ >= 0x10) {
					address_ &= 0xf;
				} else if(address_ >= 0x8 && address_ <= 0xb) {
					address_ = 0x10 + (address_ & 0x3);
				}

				// If this is a read, return a result; otherwise prepare to write.
				if(command & 0x80) {
					return (address_ & 0x100) ? seconds_[address_ & 0xff] : data_[address_];
				}

				is_writing_ = true;
				return NoResult;
			} else {
				// First test: is this to the write-protect register?
				if(address_ == 0xd) {
					write_protect_ = command;
				}

				// No other writing is permitted if the write protect
				// register won't allow it.
				if(!(write_protect_ & 0x80)) {
					if(address_ & 0x100) {
						seconds_[address_ & 0xff] = command;
					} else {
						data_[address_] = command;
					}
				}

				is_writing_ = false;
				return DidWrite;
			}
		}


	private:
		uint8_t data_[256];
		uint8_t seconds_[4];
		uint8_t write_protect_;
		bool is_writing_ = false;
		int address_;
};

/*!
	Provides the serial interface implemented by the Macintosh.
*/
class SerialClock: public ClockStorage {
	public:
		/*!
			Sets the current clock and data inputs to the clock.
		*/
		void set_input(bool clock, bool data) {
			// 	The data line is valid when the clock transitions to level 0.
			if(clock && !previous_clock_) {
				// Shift into the command_ register, no matter what.
				command_ = uint16_t((command_ << 1) | (data ? 1 : 0));
				result_ <<= 1;

				// Increment phase.
				++phase_;

				// If a whole byte has been collected, push it onwards.
				if(!(phase_&7)) {
					// Begin pessimistically.
					const auto effect = perform(uint8_t(command_));

					switch(effect) {
						case ClockStorage::NoResult:
						break;
						default:
							result_ = uint8_t(effect);
						break;
						case ClockStorage::DidWrite:
							abort();
						break;
					}
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
		int phase_ = 0;
		uint16_t command_;
		uint8_t result_ = 0;

		bool previous_clock_ = false;
};

}
}

#endif /* Apple_RealTimeClock_hpp */
