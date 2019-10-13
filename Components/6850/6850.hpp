//
//  6850.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 10/10/2019.
//  Copyright © 2019 Thomas Harte. All rights reserved.
//

#ifndef Motorola_ACIA_6850_hpp
#define Motorola_ACIA_6850_hpp

#include <cstdint>
#include "../../ClockReceiver/ClockReceiver.hpp"
#include "../../ClockReceiver/ClockingHintSource.hpp"
#include "../SerialPort/SerialPort.hpp"

namespace Motorola {
namespace ACIA {

class ACIA: public ClockingHint::Source {
	public:
		ACIA();

		/*!
			Reads from the ACIA.

			Bit 0 of the address is used as the ACIA's register select line —
			so even addresses select control/status registers, odd addresses
			select transmit/receive data registers.
		*/
		uint8_t read(int address);

		/*!
			Writes to the ACIA.

			Bit 0 of the address is used as the ACIA's register select line —
			so even addresses select control/status registers, odd addresses
			select transmit/receive data registers.
		*/
		void write(int address, uint8_t value);

		void run_for(HalfCycles);

		bool get_interrupt_line();

		// Input lines.
		Serial::Line receive;
		Serial::Line clear_to_send;
		Serial::Line data_carrier_detect;

		// Output lines.
		Serial::Line transmit;
		Serial::Line request_to_send;

	private:
		int divider_ = 1;
		enum class Parity {
			Even, Odd, None
		} parity_ = Parity::None;
		int data_bits_ = 7, stop_bits_ = 2;

		static const int NoTransmission = 0x100;
		int next_transmission_ = NoTransmission;
		void consider_transmission();

		bool receive_interrupt_enabled_ = false;
		bool transmit_interrupt_enabled_ = false;

		bool interrupt_request_ = false;

		ClockingHint::Preference preferred_clocking() final;
};

}
}

#endif /* Motorola_ACIA_6850_hpp */
