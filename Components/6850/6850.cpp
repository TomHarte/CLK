//
//  6850.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 10/10/2019.
//  Copyright © 2019 Thomas Harte. All rights reserved.
//

#include "6850.hpp"

#define LOG_PREFIX "[6850] "
#include "../../Outputs/Log.hpp"

using namespace Motorola::ACIA;

ACIA::ACIA() {}

uint8_t ACIA::read(int address) {
	if(address&1) {
		LOG("Read from receive register");
	} else {
		LOG("Read status");
		return
			((next_transmission_ == NoTransmission) ? 0x02 : 0x00) |
			(data_carrier_detect.read() ? 0x04 : 0x00) |
			(clear_to_send.read() ? 0x08 : 0x00)
		;

		// b0: receive data full.
		// b1: transmit data empty.
		// b2: DCD.
		// b3: CTS.
		// b4: framing error (i.e. no first stop bit where expected).
		// b5: receiver overran.
		// b6: parity error.
		// b7: IRQ state.
	}
	return 0x00;
}

void ACIA::write(int address, uint8_t value) {
	if(address&1) {
		next_transmission_ = value;
		consider_transmission();
	} else {
		if((value&3) == 3) {
			transmit.reset_writing();
			request_to_send.reset_writing();
		} else {
			switch(value & 3) {
				default:
				case 0: divider_ = 1;	break;
				case 1: divider_ = 16;	break;
				case 2: divider_ = 64;	break;
			}
			switch((value >> 2) & 7) {
				default:
				case 0:	data_bits_ = 7; stop_bits_ = 2; parity_ = Parity::Even;	break;
				case 1:	data_bits_ = 7; stop_bits_ = 2; parity_ = Parity::Odd;	break;
				case 2:	data_bits_ = 7; stop_bits_ = 1; parity_ = Parity::Even;	break;
				case 3:	data_bits_ = 7; stop_bits_ = 1; parity_ = Parity::Odd;	break;
				case 4:	data_bits_ = 8; stop_bits_ = 2; parity_ = Parity::None;	break;
				case 5:	data_bits_ = 8; stop_bits_ = 1; parity_ = Parity::None;	break;
				case 6:	data_bits_ = 8; stop_bits_ = 1; parity_ = Parity::Even;	break;
				case 7:	data_bits_ = 8; stop_bits_ = 1; parity_ = Parity::Odd;	break;
			}
			switch((value >> 5) & 3) {
				case 0:	request_to_send.write(false); transmit_interrupt_enabled_ = false;	break;
				case 1:	request_to_send.write(false); transmit_interrupt_enabled_ = true;	break;
				case 2:	request_to_send.write(true); transmit_interrupt_enabled_ = false;	break;
				case 3:
					request_to_send.write(false);
					transmit_interrupt_enabled_ = false;
					transmit.reset_writing();
					transmit.write(false);
				break;
			}
			receive_interrupt_enabled_ = value & 0x80;
		}
	}
}

void ACIA::run_for(HalfCycles length) {
	// Transmission.
	int transmit_advance = length.as_int();
	if(next_transmission_ != NoTransmission) {
		const auto write_data_time_remaining = transmit.write_data_time_remaining();
		if(transmit_advance > write_data_time_remaining) {
			transmit.flush_writing();
			transmit_advance -= write_data_time_remaining;
			consider_transmission();
		}
	}
	transmit.advance_writer(transmit_advance);

	// Reception.
}

void ACIA::consider_transmission() {
	if(next_transmission_ != NoTransmission && !transmit.write_data_time_remaining()) {
		// Establish start bit and [7 or 8] data bits.
		if(data_bits_ == 7) next_transmission_ &= 0x7f;
		int transmission = next_transmission_ << 1;

		// Add a parity bit, if any.
		int mask = 0x2 << data_bits_;
		if(parity_ != Parity::None) {
			next_transmission_ ^= next_transmission_ >> 4;
			next_transmission_ ^= next_transmission_ >> 2;
			next_transmission_ ^= next_transmission_ >> 1;

			if((next_transmission_&1) != (parity_ == Parity::Even)) {
				transmission |= mask;
			}
			mask <<= 1;
		}

		// Add stop bits.
		for(int c = 0; c < stop_bits_; ++c) {
			transmission |= mask;
			mask <<= 1;
		}

		// Output all that.
		const int total_bits = 1 + data_bits_ + stop_bits_ + (parity_ != Parity::None);
		transmit.write(divider_, total_bits, transmission);

		// Mark the transmit register as empty again.
		next_transmission_ = NoTransmission;
	}
}
