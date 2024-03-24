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
//		if(!scan_keyboard_) return;
		// TODO: scan_keyboard_ seems to end up in the wrong state. Investigate.

		const uint8_t prefix = is_pressed ? 0b1100'0000 : 0b1101'0000;
		enqueue(static_cast<uint8_t>(prefix | row), static_cast<uint8_t>(prefix | column));
		consider_dequeue();
	}

	void update() {
		if(serial_.events(KeyboardParty) & HalfDuplexSerial::Receive) {
			const auto reset = [&]() {
				serial_.output(KeyboardParty, HRST);
				phase_ = Phase::Idle;
			};

			const uint8_t input = serial_.input(KeyboardParty);

			// A reset command is always accepted, usurping any other state.
			if(input == HRST) {
				phase_ = Phase::ExpectingRAK1;
				event_queue_.clear();
				serial_.output(KeyboardParty, HRST);
				return;
			}

			switch(phase_) {
				case Phase::Idle:
					switch(input) {
						case RQID:	// Post keyboard ID.
							serial_.output(KeyboardParty, 0x81);	// Declare this to be a UK keyboard.
							phase_ = Phase::Idle;
						break;

						case PRST:	// "1-byte command, does nothing."
						break;

						case RQMP:
							// TODO: real mouse data.
							enqueue(0, 0);
						break;

						default:
							if((input & 0b1111'0000) == 0b0100'0000) {
								// RQPD; request to echo the low nibble.
								serial_.output(KeyboardParty, 0b1110'0000 | (input & 0b1111));
							}

							if(!(input & 0b1111'1000)) {
								// LEDS: should set LEd outputs.
							}
						break;
					}
				break;

				case Phase::ExpectingRAK1:
					if(input != RAK1) {
						reset();
						break;
					}
					serial_.output(KeyboardParty, input);
					phase_ = Phase::ExpectingRAK2;
				break;

				case Phase::ExpectingRAK2:
					if(input != RAK2) {
						reset();
						break;
					}
					serial_.output(KeyboardParty, input);
					phase_ = Phase::ExpectingACK;
				break;

				case Phase::ExpectingBACK:
					if(input != BACK) {
						reset();
						break;
					}
					phase_ = Phase::ExpectingACK;
				break;

				case Phase::ExpectingACK:
					switch(input) {
						default:
							reset();
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
					}
					phase_ = Phase::Idle;
				break;
			}

			consider_dequeue();
		}
	}

	void consider_dequeue() {
		if(phase_ == Phase::Idle) {
			dequeue_next();
		}
	}

private:
	HalfDuplexSerial &serial_;

	bool scan_keyboard_ = false;
	bool scan_mouse_ = false;
	enum class Phase {
		ExpectingRAK1,
		ExpectingRAK2,
		ExpectingBACK,
		ExpectingACK,
		Idle,
	} phase_ = Phase::Idle;

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
	static constexpr uint8_t NACK	= 0b0011'0000;	// Acknowledge for last keyboard data byte pair, selects scan/mouse mode.
	static constexpr uint8_t SACK	= 0b0011'0001;	// Last data byte acknowledge.
	static constexpr uint8_t MACK	= 0b0011'0010;	// Last data byte acknowledge.
	static constexpr uint8_t SMAK	= 0b0011'0011;	// Last data byte acknowledge.
	static constexpr uint8_t PRST	= 0b0010'0001;	// Does nothing.
};

}
