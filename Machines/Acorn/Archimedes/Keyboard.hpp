//
//  Keyboard.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 20/03/2024.
//  Copyright © 2024 Thomas Harte. All rights reserved.
//

#pragma once

#include "HalfDuplexSerial.hpp"

#include "../../../Outputs/Log.hpp"
#include "../../../Inputs/Mouse.hpp"

namespace Archimedes {

// Resource for the keyboard protocol: https://github.com/tmk/tmk_keyboard/wiki/ACORN-ARCHIMEDES-Keyboard
struct Keyboard {
	Keyboard(HalfDuplexSerial &serial) : serial_(serial) {}

	void set_key_state(int row, int column, bool is_pressed) {
		if(!scan_keyboard_) {
			logger_.info().append("Ignored key event as key scanning disabled");
			return;
		}

		// Don't waste bandwidth on repeating facts.
		if(states_[row][column] == is_pressed) return;
		states_[row][column] = is_pressed;

		// Post new key event.
		logger_.info().append("Posting row %d, column %d is now %s", row, column, is_pressed ? "pressed" : "released");
		const uint8_t prefix = is_pressed ? 0b1100'0000 : 0b1101'0000;
		enqueue(static_cast<uint8_t>(prefix | row), static_cast<uint8_t>(prefix | column));
		consider_dequeue();
	}

	void update() {
		if(serial_.events(KeyboardParty) & HalfDuplexSerial::Receive) {
			const auto reset = [&]() {
				serial_.output(KeyboardParty, HRST);
				state_ = State::Idle;
			};

			const uint8_t input = serial_.input(KeyboardParty);

			// A reset command is always accepted, usurping any other state.
			if(input == HRST) {
				logger_.info().append("HRST; resetting");
				state_ = State::ExpectingRAK1;
				event_queue_.clear();
				serial_.output(KeyboardParty, HRST);
				return;
			}

			switch(state_) {
				case State::ExpectingACK:
					if(input != NACK && input != SMAK && input != MACK && input != SACK) {
						logger_.error().append("No ack; requesting reset");
						reset();
						break;
					}
					state_ = State::Idle;
					[[fallthrough]];

				case State::Idle:
					switch(input) {
						case RQID:	// Post keyboard ID.
							serial_.output(KeyboardParty, 0x81);	// Declare this to be a UK keyboard.
							logger_.info().append("RQID; responded with 0x81");
						break;

						case PRST:	// "1-byte command, does nothing."
							logger_.info().append("PRST; ignored");
						break;

						case RQMP:
							logger_.error().append("RQMP; TODO: respond something other than 0, 0");
							enqueue(0, 0);
						break;

						case NACK:	case SMAK:	case MACK:	case SACK:
							scan_keyboard_ = input & 1;
							scan_mouse_ = input & 2;
							logger_.info().append("ACK; keyboard:%d mouse:%d", scan_keyboard_, scan_mouse_);
						break;

						default:
							if((input & 0b1111'0000) == 0b0100'0000) {
								// RQPD; request to echo the low nibble.
								serial_.output(KeyboardParty, 0b1110'0000 | (input & 0b1111));
								logger_.info().append("RQPD; echoing %x", input & 0b1111);
							} else if(!(input & 0b1111'1000)) {
								// LEDS: should set LED outputs.
								logger_.error().append("TODO: set LEDs %d%d%d", static_cast<bool>(input&4), static_cast<bool>(input&2), static_cast<bool>(input&1));
							} else {
								logger_.info().append("Ignoring unrecognised command %02x received in idle state", input);
							}
						break;
					}
				break;

				case State::ExpectingRAK1:
					if(input != RAK1) {
						logger_.info().append("Didn't get RAK1; resetting");
						reset();
						break;
					}
					logger_.info().append("Got RAK1; echoing");
					serial_.output(KeyboardParty, input);
					state_ = State::ExpectingRAK2;
				break;

				case State::ExpectingRAK2:
					if(input != RAK2) {
						logger_.info().append("Didn't get RAK2; resetting");
						reset();
						break;
					}
					logger_.info().append("Got RAK2; echoing");
					serial_.output(KeyboardParty, input);
					state_ = State::ExpectingACK;
				break;

				case State::ExpectingBACK:
					if(input != BACK) {
						logger_.info().append("Didn't get BACK; resetting");
						reset();
						break;
					}
					logger_.info().append("Got BACK; posting next byte");
					dequeue_next();
					state_ = State::ExpectingACK;
				break;
			}

			consider_dequeue();
		}
	}

	void consider_dequeue() {
		if(state_ == State::Idle && dequeue_next()) {
			state_ = State::ExpectingBACK;
		}
	}

	Inputs::Mouse &mouse() {
		return mouse_;
	}

private:
	HalfDuplexSerial &serial_;
	Log::Logger<Log::Source::Keyboard> logger_;

	bool states_[16][16]{};

	bool scan_keyboard_ = false;
	bool scan_mouse_ = false;
	enum class State {
		ExpectingRAK1,	// Post a RAK1 and proceed to ExpectingRAK2 if RAK1 is received; otherwise request a reset.
		ExpectingRAK2,	// Post a RAK2 and proceed to ExpectingACK if RAK2 is received; otherwise request a reset.
		ExpectingACK,	// Process NACK, SACK, MACK or SMAK if received; otherwise request a reset.

		Idle,			// Process any of: NACK, SACK, MACK, SMAK, RQID, RQMP, RQPD or LEDS if received; also
						// unilaterally begin post a byte pair enqueued but not yet sent if any are waiting.

		ExpectingBACK,	// Dequeue and post one further byte if BACK is received; otherwise request a reset.
	} state_ = State::Idle;

	std::vector<uint8_t> event_queue_;
	void enqueue(uint8_t first, uint8_t second) {
		event_queue_.push_back(first);
		event_queue_.push_back(second);
	}
	bool dequeue_next() {
		// To consider: a cheaper approach to the queue than this; in practice events
		// are 'rare' so it's not high priority.
		if(event_queue_.empty()) return false;
		serial_.output(KeyboardParty, event_queue_[0]);
		event_queue_.erase(event_queue_.begin());
		return true;
	}

	static constexpr uint8_t HRST	= 0b1111'1111;	// Keyboard reset.
	static constexpr uint8_t RAK1	= 0b1111'1110;	// Reset response #1.
	static constexpr uint8_t RAK2	= 0b1111'1101;	// Reset response #2.

	static constexpr uint8_t RQID	= 0b0010'0000;	// Request for keyboard ID.
	static constexpr uint8_t RQMP	= 0b0010'0010;	// Request for mouse data.

	static constexpr uint8_t BACK	= 0b0011'1111;	// Acknowledge for first keyboard data byte pair.
	static constexpr uint8_t NACK	= 0b0011'0000;	// Acknowledge for last keyboard data byte pair, disables both scanning and mouse.
	static constexpr uint8_t SACK	= 0b0011'0001;	// Last data byte acknowledge, enabling scanning but disabling mouse.
	static constexpr uint8_t MACK	= 0b0011'0010;	// Last data byte acknowledge, disabling scanning but enabling mouse.
	static constexpr uint8_t SMAK	= 0b0011'0011;	// Last data byte acknowledge, enabling scanning and mouse.
	static constexpr uint8_t PRST	= 0b0010'0001;	// Does nothing.


	struct Mouse: public Inputs::Mouse {

	};
	Mouse mouse_;
};

}
