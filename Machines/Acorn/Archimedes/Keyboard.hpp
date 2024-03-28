//
//  Keyboard.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 20/03/2024.
//  Copyright © 2024 Thomas Harte. All rights reserved.
//

#pragma once

#include "HalfDuplexSerial.hpp"

namespace Archimedes {

// Resource for the keyboard protocol: https://github.com/tmk/tmk_keyboard/wiki/ACORN-ARCHIMEDES-Keyboard
struct Keyboard {
	Keyboard(HalfDuplexSerial &serial) : serial_(serial) {}

	void set_key_state(int row, int column, bool is_pressed) {
		if(!scan_keyboard_) return;

		// Don't waste bandwidth on repeating facts.
		if(states_[row][column] == is_pressed) return;
		states_[row][column] = is_pressed;

		// Post new key event.
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
				state_ = State::ExpectingRAK1;
				event_queue_.clear();
				serial_.output(KeyboardParty, HRST);
				return;
			}

			switch(state_) {
				case State::ExpectingACK:
					if(input != NACK && input != SMAK && input != MACK && input != SACK) {
						reset();
						break;
					}
					state_ = State::Idle;
					[[fallthrough]];

				case State::Idle:
					switch(input) {
						case RQID:	// Post keyboard ID.
							serial_.output(KeyboardParty, 0x81);	// Declare this to be a UK keyboard.
							state_ = State::Idle;
						break;

						case PRST:	// "1-byte command, does nothing."
						break;

						case RQMP:
							// TODO: real mouse data.
							enqueue(0, 0);
						break;

						case NACK:
							scan_keyboard_ = scan_mouse_ = false;
						break;
						case SMAK:
							scan_keyboard_ = scan_mouse_ = true;
						break;
						case MACK:
							scan_keyboard_ = false;
							scan_mouse_ = true;
						break;
						case SACK:
							scan_keyboard_ = true;
							scan_mouse_ = false;
						break;

						default:
							if((input & 0b1111'0000) == 0b0100'0000) {
								// RQPD; request to echo the low nibble.
								serial_.output(KeyboardParty, 0b1110'0000 | (input & 0b1111));
							} else if(!(input & 0b1111'1000)) {
								// LEDS: should set LEd outputs.
							} else {
								reset();
							}
						break;
					}
				break;

				case State::ExpectingRAK1:
					if(input != RAK1) {
						reset();
						break;
					}
					serial_.output(KeyboardParty, input);
					state_ = State::ExpectingRAK2;
				break;

				case State::ExpectingRAK2:
					if(input != RAK2) {
						reset();
						break;
					}
					serial_.output(KeyboardParty, input);
					state_ = State::ExpectingACK;
				break;

				case State::ExpectingBACK:
					if(input != BACK) {
						reset();
						break;
					}
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

private:
	HalfDuplexSerial &serial_;

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
};

}
