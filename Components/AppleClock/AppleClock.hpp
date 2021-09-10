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
//			constexpr uint8_t default_data[] = {
//				0xa8, 0x00, 0x00, 0x00,
//				0xcc, 0x0a, 0xcc, 0x0a,
//				0x00, 0x00, 0x00, 0x00,
//				0x00, 0x02, 0x63, 0x00,
//				0x03, 0x88, 0x00, 0x4c
//			};
//			memcpy(data_, default_data, sizeof(default_data));
//			memset(&data_[sizeof(default_data)], 0xff, sizeof(data_) - sizeof(default_data));
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
		static constexpr uint16_t DidComplete = 0x101;
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
			switch(phase_) {
				case Phase::Command:
					// Decode an address.
					switch(command & 0x70) {
						default:
							if(command & 0x40) {
								// RAM addresses 0x00 – 0x0f.
								address_ = (command >> 2) & 0xf;
							} else return DidComplete;	// Unrecognised.
						break;

						case 0x00:
							// A time access.
							address_ = SecondsBuffer + ((command >> 2)&3);
						break;
						case 0x30:
							// Either a register access or an extended instruction.
							if(command & 0x08) {
								address_ = (command & 0x7) << 5;
								phase_ = (command & 0x80) ? Phase::SecondAddressByteRead : Phase::SecondAddressByteWrite;
								return NoResult;
							} else {
								address_ = (command & 4) ? RegisterWriteProtect : RegisterTest;
							}
						break;
						case 0x20:
							// RAM addresses 0x10 – 0x13.
							address_ = 0x10 + ((command >> 2) & 0x3);
						break;
					}

					// If this is a read, return a result; otherwise prepare to write.
					if(command & 0x80) {
						// The two registers are write-only.
						if(address_ == RegisterTest || address_ == RegisterWriteProtect) {
							return DidComplete;
						}
						return (address_ >= SecondsBuffer) ? seconds_[address_ & 0xff] : data_[address_];
					}
					phase_ = Phase::WriteData;
				return NoResult;

				case Phase::SecondAddressByteRead:
				case Phase::SecondAddressByteWrite:
					if(command & 0x83) {
						return DidComplete;
					}
					address_ |= command >> 2;

					if(phase_ == Phase::SecondAddressByteRead) {
						phase_ = Phase::Command;
						return data_[address_];	// Only RAM accesses can get this far.
					} else {
						phase_ = Phase::WriteData;
					}
				return NoResult;

				case Phase::WriteData:
					// First test: is this to the write-protect register?
					if(address_ == RegisterWriteProtect) {
						write_protect_ = command;
						return DidComplete;
					}

					if(address_ == RegisterTest) {
						// No documentation here.
						return DidComplete;
					}

					// No other writing is permitted if the write protect
					// register won't allow it.
					if(!(write_protect_ & 0x80)) {
						if(address_ >= SecondsBuffer) {
							seconds_[address_ & 0xff] = command;
						} else {
							data_[address_] = command;
						}
					}

					phase_ = Phase::Command;
				return DidComplete;
			}

			return NoResult;
		}


	private:
		uint8_t data_[256]{};
		uint8_t seconds_[4]{};
		uint8_t write_protect_ = 0;
		int address_ = 0;

		static constexpr int SecondsBuffer = 0x100;
		static constexpr int RegisterTest = 0x200;
		static constexpr int RegisterWriteProtect = 0x201;

		enum class Phase {
			Command,
			SecondAddressByteRead,
			SecondAddressByteWrite,
			WriteData
		};
		Phase phase_ = Phase::Command;

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
						case ClockStorage::DidComplete:
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

/*!
	Provides the parallel interface implemented by the IIgs.
*/
class ParallelClock: public ClockStorage {
	public:
		void set_control(uint8_t control) {
			if(!(control&0x80)) return;

			if(control & 0x40) {
				// Read from the RTC.
				// A no-op for now.
			} else {
				// Write to the RTC. Which in this implementation also sets up a future read.
				const auto result = perform(data_);
				if(result < 0x100) {
					data_ = uint8_t(result);
				}
			}

			// MAGIC! The transaction took 0 seconds.
			// TODO: no magic.
			control_ = control & 0x7f;

			// Bit 5 is also meant to be 1 or 0 to indicate the final byte.
		}

		uint8_t get_control() {
			return control_;
		}

		void set_data(uint8_t data) {
			data_ = data;
		}

		uint8_t get_data() {
			return data_;
		}

	private:
		uint8_t data_;
		uint8_t control_;
};

}
}

#endif /* Apple_RealTimeClock_hpp */
