//
//  Keyboard.h
//  Clock Signal
//
//  Created by Thomas Harte on 08/05/2019.
//  Copyright © 2019 Thomas Harte. All rights reserved.
//

#ifndef Apple_Macintosh_Keyboard_hpp
#define Apple_Macintosh_Keyboard_hpp

#include "../../KeyboardMachine.hpp"
#include "../../../ClockReceiver/ClockReceiver.hpp"

#include <mutex>
#include <vector>

namespace Apple {
namespace Macintosh {

constexpr uint16_t KeypadMask = 0x100;

/*!
	Defines the keycodes that could be passed directly to a Macintosh via set_key_pressed.
*/
enum class Key: uint16_t {
	/*
		See p284 of the Apple Guide to the Macintosh Family Hardware
		for documentation of the mapping below.
	*/
	BackTick = 0x65,
	k1 = 0x25,	k2 = 0x27,	k3 = 0x29,	k4 = 0x2b,	k5 = 0x2f,
	k6 = 0x2d,	k7 = 0x35,	k8 = 0x39,	k9 = 0x33,	k0 = 0x3b,

	Hyphen = 0x37,
	Equals = 0x31,
	Backspace = 0x67,
	Tab = 0x61,

	Q = 0x19, W = 0x1b, E = 0x1d, R = 0x1f, T = 0x23, Y = 0x21, U = 0x41, I = 0x45, O = 0x3f, P = 0x47,
	A = 0x01, S = 0x03, D = 0x05, F = 0x07, G = 0x0b, H = 0x09, J = 0x4d, K = 0x51, L = 0x4b,
	Z = 0x0d, X = 0x0f, C = 0x11, V = 0x13, B = 0x17, N = 0x5b, M = 0x5d,

	OpenSquareBracket = 0x43,
	CloseSquareBracket = 0x3d,
	Semicolon = 0x53,
	Quote = 0x4f,
	Comma = 0x57,
	FullStop = 0x5f,
	ForwardSlash = 0x59,

	CapsLock = 0x73,
	Shift = 0x71,
	Option = 0x75,
	Command = 0x6f,

	Space = 0x63,
	Backslash = 0x55,
	Return = 0x49,

	Left = KeypadMask | 0x0d,
	Right = KeypadMask | 0x05,
	Up = KeypadMask | 0x1b,
	Down = KeypadMask | 0x11,

	KeypadDelete = KeypadMask | 0x0f,
	KeypadEquals = KeypadMask | 0x11,
	KeypadSlash = KeypadMask | 0x1b,
	KeypadAsterisk = KeypadMask | 0x05,
	KeypadMinus = KeypadMask | 0x1d,
	KeypadPlus = KeypadMask | 0x0d,
	KeypadEnter = KeypadMask | 0x19,
	KeypadDecimalPoint = KeypadMask | 0x03,

	Keypad9 = KeypadMask | 0x39,
	Keypad8 = KeypadMask | 0x37,
	Keypad7 = KeypadMask | 0x33,
	Keypad6 = KeypadMask | 0x31,
	Keypad5 = KeypadMask | 0x2f,
	Keypad4 = KeypadMask | 0x2d,
	Keypad3 = KeypadMask | 0x2b,
	Keypad2 = KeypadMask | 0x29,
	Keypad1 = KeypadMask | 0x27,
	Keypad0 = KeypadMask | 0x25
};

class Keyboard {
	public:
		void set_input(bool data) {
			switch(mode_) {
				case Mode::Waiting:
					/*
						"Only the computer can initiate communication over the keyboard lines. When the computer and keyboard
						are turned on, the computer is in charge of the keyboard interface and the keyboard is passive. The
						computer signals that it is ready to begin communication by pulling the Keyboard Data line low."
					*/
					if(!data) {
						mode_ = Mode::AcceptingCommand;
						phase_ = 0;
						command_ = 0;
					}
				break;

				case Mode::AcceptingCommand:
					/* Note value, so that it can be latched upon a clock transition. */
					data_input_ = data;
				break;

				case Mode::AwaitingEndOfCommand:
					/*
						The last bit of the command leaves the Keyboard Data line low; the computer then indicates that it is ready
						to receive the keyboard's response by setting the Keyboard Data line high.
					*/
					if(data) {
						mode_ = Mode::PerformingCommand;
						phase_ = 0;
					}
				break;

				default:
				case Mode::SendingResponse:
					/* This line isn't currently an input; do nothing. */
				break;
			}
		}

		bool get_clock() {
			return clock_output_;
		}

		bool get_data() {
			return !!(response_ & 0x80);
		}

		/*!
			The keyboard expects ~10 µs-frequency ticks, i.e. a clock rate of just around 100 kHz.
		*/
		void run_for(HalfCycles cycle) {
			switch(mode_) {
				default:
				case Mode::Waiting: return;

				case Mode::AcceptingCommand: {
					/*
						"When the computer is sending data to the keyboard, the keyboard transmits eight cycles of 400 µS each (180 µS low,
						220 µS high) on the Keyboard Clock line. On the falling edge of each keyboard clock cycle, the Macintosh Plus places
						a data bit on the data line and holds it there for 400 µS. The keyboard reads the data bit 80 µS after the rising edge
						of the Keyboard Clock signal."
					*/
					const auto offset = phase_ % 40;
					clock_output_ = offset >= 18;

					if(offset == 26) {
						command_ = (command_ << 1) | (data_input_ ? 1 : 0);
					}

					++phase_;
					if(phase_ == 8*40) {
						mode_ = Mode::AwaitingEndOfCommand;
						phase_ = 0;
						clock_output_ = false;
					}
				} break;

				case Mode::AwaitingEndOfCommand:
					// Time out if the end-of-command seems not to be forthcoming.
					// This is an elaboration on my part; a guess.
					++phase_;
					if(phase_ == 1000) {
						clock_output_ = false;
						mode_ = Mode::Waiting;
						phase_ = 0;
					}
				return;

				case Mode::PerformingCommand: {
					response_ = perform_command(command_);

					// Inquiry has a 0.25-second timeout; everything else is instant.
					++phase_;
					if(phase_ == 25000 || command_ != 0x10 || response_ != 0x7b) {
						mode_ = Mode::SendingResponse;
						phase_ = 0;
					}
				} break;

				case Mode::SendingResponse: {
					/*
						"When sending data to the computer, the keyboard transmits eight cycles of 330 µS each (160 µS low, 170 µS high)
						on the normally high Keyboard Clock line. It places a data bit on the data line 40 µS before the falling edge of each
						clock cycle and maintains it for 330 µS. The VIA in the computer latches the data bit into its shift register on the
						rising edge of the Keyboard Clock signal."
					*/
					const auto offset = phase_ % 33;
					clock_output_ = offset >= 16;

					if(offset == 29) {
						response_ <<= 1;
					}

					++phase_;
					if(phase_ == 8*33) {
						clock_output_ = false;
						mode_ = Mode::Waiting;
						phase_ = 0;
					}
				} break;
			}
		}

		void enqueue_key_state(uint16_t key, bool is_pressed) {
			// Front insert; messages will be pop_back'd.
			std::lock_guard<decltype(key_queue_mutex_)> lock(key_queue_mutex_);

			// Keys on the keypad are preceded by a $79 keycode; in the internal naming scheme
			// they are indicated by having bit 8 set. So add the $79 prefix if required.
			if(key & KeypadMask) {
				key_queue_.insert(key_queue_.begin(), 0x79);
			}
			key_queue_.insert(key_queue_.begin(), (is_pressed ? 0x00 : 0x80) | uint8_t(key));
		}

	private:
		/// Performs the pre-ADB Apple keyboard protocol command @c command, returning
		/// the proper result if the command were to terminate now. So, it treats inquiry
		/// and instant as the same command.
		int perform_command(int command) {
			switch(command) {
				case 0x10:		// Inquiry.
				case 0x14: {	// Instant.
					std::lock_guard<decltype(key_queue_mutex_)> lock(key_queue_mutex_);
					if(!key_queue_.empty()) {
						const auto new_message = key_queue_.back();
						key_queue_.pop_back();
						return new_message;
					}
				} break;

				case 0x16:	// Model number.
				return
					0x01 |			// b0: always 1
					(1 << 1) |		// keyboard model number
					(1 << 4);		// next device number
									// (b7 not set => no next device)

				case 0x36:	// Test
				return 0x7d;		// 0x7d = ACK, 0x77 = not ACK.
			}
			return 0x7b;	// No key transition.
		}

		/// Maintains the current operating mode — a record of what the
		/// keyboard is doing now.
		enum class Mode {
			/// The keyboard is waiting to begin a transaction.
			Waiting,
			/// The keyboard is currently clocking in a new command.
			AcceptingCommand,
			/// The keyboard is waiting for the computer to indicate that it is ready for a response.
			AwaitingEndOfCommand,
			/// The keyboard is in the process of performing the command it most-recently received.
			/// If the command was an 'inquiry', this state may persist for a non-neglibible period of time.
			PerformingCommand,
			/// The keyboard is currently shifting a response back to the computer.
			SendingResponse,
		} mode_ = Mode::Waiting;

		/// Holds a count of progress through the current @c Mode. Exact meaning depends on mode.
		int phase_ = 0;
		/// Holds the most-recently-received command; the command is shifted into here as it is received
		/// so this may not be valid prior to Mode::PerformingCommand.
		int command_ = 0;
		/// Populated during PerformingCommand as the response to the most-recently-received command, this
		/// is then shifted out to teh host computer. So it is guaranteed valid at the beginning of Mode::SendingResponse,
		/// but not afterwards.
		int response_ = 0;

		/// The current state of the serial connection's data input.
		bool data_input_ = false;
		/// The current clock output from this keyboard.
		bool clock_output_ = false;

		/// Guards multithread access to key_queue_.
		std::mutex key_queue_mutex_;
		/// A FIFO queue for key events, in the form they'd be communicated to the Macintosh,
		/// with the newest events towards the front.
		std::vector<uint8_t> key_queue_;
};

/*!
	Provides a mapping from idiomatic PC keys to Macintosh keys.
*/
class KeyboardMapper: public KeyboardMachine::MappedMachine::KeyboardMapper {
	uint16_t mapped_key_for_key(Inputs::Keyboard::Key key) final;
};

}
}

#endif /* Apple_Macintosh_Keyboard_hpp */
