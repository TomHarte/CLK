//
//  Keyboard.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 20/03/2024.
//  Copyright © 2024 Thomas Harte. All rights reserved.
//

#pragma once

#include "HalfDuplexSerial.hpp"

#include "Outputs/Log.hpp"
#include "Inputs/Mouse.hpp"

#include <bitset>

namespace Archimedes {

namespace {
constexpr uint16_t map(int row, int column) {
	return static_cast<uint16_t>((row << 4) | column);
}

constexpr uint8_t row(uint16_t key) {
	return static_cast<uint8_t>(key >> 4);
}

constexpr uint8_t column(uint16_t key) {
	return static_cast<uint8_t>(key & 0xf);
}
}

struct Key {
	/// Named key codes that the machine wlll accept directly.
	enum Value: uint16_t {
		Escape = map(0, 0),					F1 = map(0, 1),					F2 = map(0, 2),					F3 = map(0, 3),
		F4 = map(0, 4),						F5 = map(0, 5),					F6 = map(0, 6),					F7 = map(0, 7),
		F8 = map(0, 8),						F9 = map(0, 9),					F10 = map(0, 10),				F11 = map(0, 11),
		F12 = map(0, 12),					Print = map(0, 13),				Scroll = map(0, 14),			Break = map(0, 15),

		Tilde = map(1, 0),					k1 = map(1, 1),					k2 = map(1, 2),					k3 = map(1, 3),
		k4 = map(1, 4),						k5 = map(1, 5),					k6 = map(1, 6),					k7 = map(1, 7),
		k8 = map(1, 8),						k9 = map(1, 9),					k0 = map(1, 10),				Hyphen = map(1, 11),
		Equals = map(1, 12),				GBPound = map(1, 13),			Backspace = map(1, 14),			Insert = map(1, 15),

		Home = map(2, 0),					PageUp = map(2, 1),				NumLock = map(2, 2),			KeypadSlash = map(2, 3),
		KeypadAsterisk = map(2, 4),			KeypadHash = map(2, 5),			Tab = map(2, 6),				Q = map(2, 7),
		W = map(2, 8),						E = map(2, 9),					R = map(2, 10),					T = map(2, 11),
		Y = map(2, 12),						U = map(2, 13),					I = map(2, 14),					O = map(2, 15),

		P = map(3, 0),						OpenSquareBracket = map(3, 1),	CloseSquareBracket = map(3, 2),	Backslash = map(3, 3),
		Delete = map(3, 4),					Copy = map(3, 5),				PageDown = map(3, 6),			Keypad7 = map(3, 7),
		Keypad8 = map(3, 8),				Keypad9 = map(3, 9),			KeypadMinus = map(3, 10),		LeftControl = map(3, 11),
		A = map(3, 12),						S = map(3, 13),					D = map(3, 14),					F = map(3, 15),

		G = map(4, 0),						H = map(4, 1),					J = map(4, 2),					K = map(4, 3),
		L = map(4, 4),						Semicolon = map(4, 5),			Quote = map(4, 6),				Return = map(4, 7),
		Keypad4 = map(4, 8),				Keypad5 = map(4, 9),			Keypad6 = map(4, 10),			KeypadPlus = map(4, 11),
		LeftShift = map(4, 12),				/* unused */					Z = map(4, 14),					X = map(4, 15),

		C = map(5, 0),						V = map(5, 1),					B = map(5, 2),					N = map(5, 3),
		M = map(5, 4),						Comma = map(5, 5),				FullStop = map(5, 6),			ForwardSlash = map(5, 7),
		RightShift = map(5, 8),				Up = map(5, 9),					Keypad1 = map(5, 10),			Keypad2 = map(5, 11),
		Keypad3 = map(5, 12),				CapsLock = map(5, 13),			LeftAlt = map(5, 14),			Space = map(5, 15),

		RightAlt = map(6, 0),				RightControl = map(6, 1),		Left = map(6, 2),				Down = map(6, 3),
		Right = map(6, 4),					Keypad0 = map(6, 5),			KeypadDecimalPoint = map(6, 6),	KeypadEnter = map(6, 7),

		Max = KeypadEnter,
	};
};

// Resource for the keyboard protocol: https://github.com/tmk/tmk_keyboard/wiki/ACORN-ARCHIMEDES-Keyboard
struct Keyboard {
	Keyboard(HalfDuplexSerial &serial) : serial_(serial), mouse_(*this) {}

	void set_key_state(uint16_t key, bool is_pressed) {
		states_[key] = is_pressed;

		if(!scan_keyboard_) {
			Logger::info().append("Ignored key event as key scanning disabled");
			return;
		}

		// Don't waste bandwidth on repeating facts.
		if(posted_states_[key] == is_pressed) return;

		// Post new key event.
		enqueue_key_event(key, is_pressed);
		consider_dequeue();
	}

	void set_mouse_button(uint8_t button, bool is_pressed) {
		if(!scan_mouse_) {
			return;
		}

		// Post new key event.
		enqueue_key_event(7, button, is_pressed);
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
				Logger::info().append("HRST; resetting");
				state_ = State::ExpectingRAK1;
				event_queue_.clear();
				serial_.output(KeyboardParty, HRST);
				return;
			}

			switch(state_) {
				case State::ExpectingACK:
					if(input != NACK && input != SMAK && input != MACK && input != SACK) {
						Logger::error().append("No ack; requesting reset");
						reset();
						break;
					}
					state_ = State::Idle;
					[[fallthrough]];

				case State::Idle:
					switch(input) {
						case RQID:	// Post keyboard ID.
							serial_.output(KeyboardParty, 0x81);	// Declare this to be a UK keyboard.
							Logger::info().append("RQID; responded with 0x81");
						break;

						case PRST:	// "1-byte command, does nothing."
							Logger::info().append("PRST; ignored");
						break;

						case RQMP:
							Logger::error().append("RQMP; TODO: respond something other than 0, 0");
							enqueue(0, 0);
						break;

						case NACK:	case SMAK:	case MACK:	case SACK: {
							const bool was_scanning_keyboard = input & 1;
							scan_keyboard_ = input & 1;
							if(!scan_keyboard_) {
								posted_states_.reset();
							} else if(!was_scanning_keyboard) {
								needs_state_check_ = true;
							}
							scan_mouse_ = input & 2;
							Logger::info().append("ACK; keyboard:%d mouse:%d", scan_keyboard_, scan_mouse_);
						} break;

						default:
							if((input & 0b1111'0000) == 0b0100'0000) {
								// RQPD; request to echo the low nibble.
								serial_.output(KeyboardParty, 0b1110'0000 | (input & 0b1111));
								Logger::info().append("RQPD; echoing %x", input & 0b1111);
							} else if(!(input & 0b1111'1000)) {
								// LEDS: should set LED outputs.
								Logger::error().append("TODO: set LEDs %d%d%d", static_cast<bool>(input&4), static_cast<bool>(input&2), static_cast<bool>(input&1));
							} else {
								Logger::info().append("Ignoring unrecognised command %02x received in idle state", input);
							}
						break;
					}
				break;

				case State::ExpectingRAK1:
					if(input != RAK1) {
						Logger::info().append("Didn't get RAK1; resetting");
						reset();
						break;
					}
					Logger::info().append("Got RAK1; echoing");
					serial_.output(KeyboardParty, input);
					state_ = State::ExpectingRAK2;
				break;

				case State::ExpectingRAK2:
					if(input != RAK2) {
						Logger::info().append("Didn't get RAK2; resetting");
						reset();
						break;
					}
					Logger::info().append("Got RAK2; echoing");
					serial_.output(KeyboardParty, input);
					state_ = State::ExpectingACK;
				break;

				case State::ExpectingBACK:
					if(input != BACK) {
						Logger::info().append("Didn't get BACK; resetting");
						reset();
						break;
					}
					Logger::info().append("Got BACK; posting next byte");
					dequeue_next();
					state_ = State::ExpectingACK;
				break;
			}
		}

		consider_dequeue();
	}

	void consider_dequeue() {
		if(state_ == State::Idle) {
			// If the key event queue is empty but keyboard scanning is enabled, check for
			// any disparity between posted keys states and actuals.
			if(needs_state_check_) {
				needs_state_check_ = false;
				if(states_ != posted_states_) {
					for(size_t key = 0; key < Key::Max; key++) {
						if(states_[key] != posted_states_[key]) {
							enqueue_key_event(static_cast<uint16_t>(key), states_[key]);
						}
					}
				}
			}

			// If the key event queue is _still_ empty, grab as much mouse motion
			// as available.
			if(event_queue_.empty()) {
				const int x = std::clamp(mouse_x_, -0x3f, 0x3f);
				const int y = std::clamp(mouse_y_, -0x3f, 0x3f);
				mouse_x_ -= x;
				mouse_y_ -= y;

				if(x || y) {
					enqueue(static_cast<uint8_t>(x) & 0x7f, static_cast<uint8_t>(-y) & 0x7f);
				}
			}

			if(dequeue_next()) {
				state_ = State::ExpectingBACK;
			}
		}
	}

	Inputs::Mouse &mouse() {
		return mouse_;
	}

private:
	HalfDuplexSerial &serial_;
	using Logger = Log::Logger<Log::Source::Keyboard>;

	std::bitset<Key::Max> states_;
	std::bitset<Key::Max> posted_states_;
	bool needs_state_check_ = false;

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
	void enqueue_key_event(uint16_t key, bool is_pressed) {
		posted_states_[key] = is_pressed;
		enqueue_key_event(row(key), column(key), is_pressed);
	}
	void enqueue_key_event(uint8_t row, uint8_t column, bool is_pressed) {
		Logger::info().append("Posting row %d, column %d is now %s", row, column, is_pressed ? "pressed" : "released");
		const uint8_t prefix = is_pressed ? 0b1100'0000 : 0b1101'0000;
		enqueue(static_cast<uint8_t>(prefix | row), static_cast<uint8_t>(prefix | column));
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
		Mouse(Keyboard &keyboard): keyboard_(keyboard) {}

		void move(int x, int y) override {
			keyboard_.mouse_x_ += x;
			keyboard_.mouse_y_ += y;
		}

		int get_number_of_buttons() const override {
			return 3;
		}

		virtual void set_button_pressed(int index, bool is_pressed) override {
			keyboard_.set_mouse_button(static_cast<uint8_t>(index), is_pressed);
		}

	private:
		Keyboard &keyboard_;
	};
	Mouse mouse_;

	int mouse_x_ = 0;
	int mouse_y_ = 0;
};

}
