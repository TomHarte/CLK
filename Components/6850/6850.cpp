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

uint8_t ACIA::read(int address) {
	if(address&1) {
		LOG("Read from receive register");
	} else {
		LOG("Read status");
		return status_;
	}
	return 0x00;
}

void ACIA::write(int address, uint8_t value) {
	if(address&1) {
		LOG("Write to transmit register");
	} else {
		if((value&3) == 3) {
			LOG("Reset");
		} else {
			switch(value & 3) {
				default:
				case 0: divider_ = 1;	break;
				case 1: divider_ = 16;	break;
				case 2: divider_ = 64;	break;
			}
			switch((value >> 2) & 7) {
				default:
				case 0:	word_size_ = 7; stop_bits_ = 2; parity_ = Parity::Even;	break;
				case 1:	word_size_ = 7; stop_bits_ = 2; parity_ = Parity::Odd;	break;
				case 2:	word_size_ = 7; stop_bits_ = 1; parity_ = Parity::Even;	break;
				case 3:	word_size_ = 7; stop_bits_ = 1; parity_ = Parity::Odd;	break;
				case 4:	word_size_ = 8; stop_bits_ = 2; parity_ = Parity::None;	break;
				case 5:	word_size_ = 8; stop_bits_ = 1; parity_ = Parity::None;	break;
				case 6:	word_size_ = 8; stop_bits_ = 1; parity_ = Parity::Even;	break;
				case 7:	word_size_ = 8; stop_bits_ = 1; parity_ = Parity::Odd;	break;
			}
			switch((value >> 5) & 3) {
				case 0:	set_ready_to_transmit(false); transmit_interrupt_enabled_ = false;	break;
				case 1:	set_ready_to_transmit(false); transmit_interrupt_enabled_ = true;	break;
				case 2:	set_ready_to_transmit(true); transmit_interrupt_enabled_ = false;	break;
				case 3:	set_ready_to_transmit(false); transmit_interrupt_enabled_ = false;	break;	/* TODO: transmit a break level on the transmit output. */
			}
			receive_interrupt_enabled_ = value & 0x80;
			LOG("Write to control register");
		}
	}
}

void ACIA::run_for(HalfCycles) {
}

void ACIA::set_ready_to_transmit(bool) {

}
