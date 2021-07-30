//
//  Keyboard.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 29/07/2021.
//  Copyright © 2021 Thomas Harte. All rights reserved.
//

#include "Keyboard.hpp"

// Notes to self:
//
//
// Before
// the transmission starts, both KCLK and KDAT are high.  The keyboard starts
// the transmission by putting out the first data bit (on KDAT), followed by
// a pulse on KCLK (low then high); then it puts out the second data bit and
// pulses KCLK until all eight data bits have been sent.
//
// When the computer has received the eighth bit, it must pulse KDAT low for
// at least 1 (one) microsecond, as a handshake signal to the keyboard. The
// keyboard must be able to detect pulses greater than or equal
// to 1 microsecond.  Software MUST pulse the line low for 85 microseconds to
// ensure compatibility with all keyboard models.
//
//
// If the handshake pulse does not arrive within
// 143 ms of the last clock of the transmission, the keyboard will assume
// that the computer is still waiting for the rest of the transmission and is
// therefore out of sync.  The keyboard will then attempt to restore sync by
// going into "resync mode."  In this mode, the keyboard clocks out a 1 and
// waits for a handshake pulse. If none arrives within 143 ms, it clocks out
// another 1 and waits again.
//
// The keyboard Hard Resets the Amiga by pulling KCLK low and starting a 500
// millisecond timer.   When one or more of the keys is released and 500
// milliseconds have passed, the keyboard will release KCLK.
//
// The usual sequence of events will therefore be:  power-up; synchronize;
// transmit "initiate power-up key stream" ($FD); transmit "terminate key
// stream" ($FE).

using namespace Amiga;

uint8_t Keyboard::update(uint8_t input) {
	// If a bit transmission is ongoing, continue that, up to and including
	// the handshake. If no handshake comes, set a macro state of synchronising.
	switch(shift_state_) {
		case ShiftState::Shifting:
			// The keyboard processor sets the KDAT line about 20 microseconds before it
			// pulls KCLK low.  KCLK stays low for about 20 microseconds, then goes high
			// again.  The processor waits another 20 microseconds before changing KDAT.
			switch(bit_phase_) {
				default: break;
				case 0:		lines_ = Lines::Clock | (shift_sequence_ & 1);	break;
				case 20:	lines_ = (shift_sequence_ & 1);					break;
				case 40:	lines_ = Lines::Clock | (shift_sequence_ & 1);	break;
			}
			bit_phase_ = (bit_phase_ + 1) % 60;

			if(!bit_phase_) {
				--bits_remaining_;
				shift_sequence_ >>= 1;
				if(!bits_remaining_) {
					shift_state_ = ShiftState::AwaitingHandshake;
				}
			}
		return lines_;

		case ShiftState::AwaitingHandshake:
			if(!(input & Lines::Data)) {
				shift_state_ = ShiftState::Idle;
			}
			++bit_phase_;
			if(bit_phase_ == 143) {
//				shift_state_ = ShiftState::Synchronising;
			}
		return lines_;

		default: break;
	}

	switch(state_) {
		case State::Startup:
			bit_phase_ = 0;
			shift_sequence_ = 0xff;
			shift_state_ = ShiftState::Shifting;
		break;
	}

	return lines_;
}
