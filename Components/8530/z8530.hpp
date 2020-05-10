//
//  z8530.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 07/06/2019.
//  Copyright © 2019 Thomas Harte. All rights reserved.
//

#ifndef z8530_hpp
#define z8530_hpp

#include <cstdint>

namespace Zilog {
namespace SCC {

/*!
	Models the Zilog 8530 SCC, a serial adaptor.
*/
class z8530 {
	public:
		/*
			**Interface for emulated machine.**

			Notes on addressing below:

			There's no inherent ordering of the two 'address' lines,
			A/B and C/D, but the methods below assume:

				A/B = A0
				C/D = A1
		*/
		std::uint8_t read(int address);
		void write(int address, std::uint8_t value);
		void reset();
		bool get_interrupt_line() const;

		struct Delegate {
			virtual void did_change_interrupt_status(z8530 *, bool new_status) = 0;
		};
		void set_delegate(Delegate *delegate) {
			delegate_ = delegate;
		}

		/*
			**Interface for serial port input.**
		*/
		void set_dcd(int port, bool level);

	private:
		class Channel {
			public:
				uint8_t read(bool data, uint8_t pointer);
				void write(bool data, uint8_t pointer, uint8_t value);
				void set_dcd(bool level);
				bool get_interrupt_line() const;

			private:
				uint8_t data_ = 0xff;

				enum class Parity {
					Even, Odd, Off
				} parity_ = Parity::Off;

				enum class StopBits {
					Synchronous, OneBit, OneAndAHalfBits, TwoBits
				} stop_bits_ = StopBits::Synchronous;

				enum class Sync {
					Monosync, Bisync, SDLC, External
				} sync_mode_ = Sync::Monosync;

				int clock_rate_multiplier_ = 1;

				uint8_t interrupt_mask_ = 0;			// i.e. Write Register 0x1.

				uint8_t external_interrupt_mask_ = 0;	// i.e. Write Register 0xf.
				bool external_status_interrupt_ = false;
				uint8_t external_interrupt_status_ = 0;

				bool dcd_ = false;
		} channels_[2];

		uint8_t pointer_ = 0;

		uint8_t interrupt_vector_ = 0;

		uint8_t master_interrupt_control_ = 0;

		bool previous_interrupt_line_ = false;
		void update_delegate();
		Delegate *delegate_ = nullptr;
};

}
}


#endif /* z8530_hpp */
