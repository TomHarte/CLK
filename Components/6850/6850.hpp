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

namespace Motorola {
namespace ACIA {

class ACIA {
	public:
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

	private:
		int divider_ = 1;
		uint8_t status_ = 0x00;
		enum class Parity {
			Even, Odd, None
		} parity_ = Parity::None;
		int word_size_ = 7, stop_bits_ = 2;
		bool receive_interrupt_enabled_ = false;
		bool transmit_interrupt_enabled_ = false;

		void set_ready_to_transmit(bool);
};

}
}

#endif /* Motorola_ACIA_6850_hpp */
