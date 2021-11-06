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

Keyboard::Keyboard(Serial::Line &output) : output_(output) {
	output_.set_writer_clock_rate(HalfCycles(1'000'000));	// Use µs.
}

/*uint8_t Keyboard::update(uint8_t input) {
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
}*/

void Keyboard::set_key_state(uint16_t key, bool is_pressed) {
	output_.write<false>(
		HalfCycles(60),
		uint8_t(((key << 1) | (is_pressed ? 0 : 1)) ^ 0xff)
	);
}

void Keyboard::clear_all_keys() {
}

// MARK: - KeyboardMapper.

uint16_t KeyboardMapper::mapped_key_for_key(Inputs::Keyboard::Key key) const {
#define BIND(source, dest)	case Inputs::Keyboard::Key::source:	return uint16_t(Key::dest)
#define DIRECTBIND(source)	BIND(source, source)
	switch(key) {
		default: break;

		DIRECTBIND(Escape);
		DIRECTBIND(Delete);

		DIRECTBIND(F1);	DIRECTBIND(F2);	DIRECTBIND(F3);	DIRECTBIND(F4);	DIRECTBIND(F5);
		DIRECTBIND(F6);	DIRECTBIND(F7);	DIRECTBIND(F8);	DIRECTBIND(F9);	DIRECTBIND(F10);

		BIND(BackTick, Tilde);
		DIRECTBIND(k1);	DIRECTBIND(k2);	DIRECTBIND(k3);	DIRECTBIND(k4);	DIRECTBIND(k5);
		DIRECTBIND(k6);	DIRECTBIND(k7);	DIRECTBIND(k8);	DIRECTBIND(k9);	DIRECTBIND(k0);

		DIRECTBIND(Hyphen);
		DIRECTBIND(Equals);
		DIRECTBIND(Backslash);
		DIRECTBIND(Backspace);
		DIRECTBIND(Tab);
		DIRECTBIND(CapsLock);

		BIND(LeftControl, Control);
		BIND(RightControl, Control);
		DIRECTBIND(LeftShift);
		DIRECTBIND(RightShift);
		BIND(LeftOption, Alt);
		BIND(RightOption, Alt);
		BIND(LeftMeta, LeftAmiga);
		BIND(RightMeta, RightAmiga);

		DIRECTBIND(Q);	DIRECTBIND(W);	DIRECTBIND(E);	DIRECTBIND(R);	DIRECTBIND(T);
		DIRECTBIND(Y);	DIRECTBIND(U);	DIRECTBIND(I);	DIRECTBIND(O);	DIRECTBIND(P);
		DIRECTBIND(A);	DIRECTBIND(S);	DIRECTBIND(D);	DIRECTBIND(F);	DIRECTBIND(G);
		DIRECTBIND(H);	DIRECTBIND(J);	DIRECTBIND(K);	DIRECTBIND(L);	DIRECTBIND(Z);
		DIRECTBIND(X);	DIRECTBIND(C);	DIRECTBIND(V);	DIRECTBIND(B);	DIRECTBIND(N);
		DIRECTBIND(M);

		DIRECTBIND(OpenSquareBracket);
		DIRECTBIND(CloseSquareBracket);

		DIRECTBIND(Help);
		BIND(Insert, Help);
		BIND(Home, Help);
		BIND(End, Help);
		BIND(Enter, Return);
		DIRECTBIND(Semicolon);
		DIRECTBIND(Quote);
		DIRECTBIND(Comma);
		DIRECTBIND(FullStop);
		DIRECTBIND(ForwardSlash);

		DIRECTBIND(Space);
		DIRECTBIND(Up);
		DIRECTBIND(Down);
		DIRECTBIND(Left);
		DIRECTBIND(Right);

		DIRECTBIND(Keypad0);	DIRECTBIND(Keypad1);	DIRECTBIND(Keypad2);
		DIRECTBIND(Keypad3);	DIRECTBIND(Keypad4);	DIRECTBIND(Keypad5);
		DIRECTBIND(Keypad6);	DIRECTBIND(Keypad7);	DIRECTBIND(Keypad8);
		DIRECTBIND(Keypad9);

		DIRECTBIND(KeypadDecimalPoint);
		DIRECTBIND(KeypadMinus);
		DIRECTBIND(KeypadEnter);
	}
#undef DIRECTBIND
#undef BIND
	return MachineTypes::MappedKeyboardMachine::KeyNotMapped;
}
