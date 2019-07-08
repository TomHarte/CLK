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

#include <mutex>
#include <vector>

namespace Apple {
namespace Macintosh {

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
			key_queue_.insert(key_queue_.begin(), (is_pressed ? 0x00 : 0x80) | uint8_t(key));
		}

	private:

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

		enum class Mode {
			Waiting,
			AcceptingCommand,
			AwaitingEndOfCommand,
			SendingResponse,
			PerformingCommand
		} mode_ = Mode::Waiting;
		int phase_ = 0;
		int command_ = 0;
		int response_ = 0;

		bool data_input_ = false;
		bool clock_output_ = false;

		// TODO: improve this very, very simple implementation.
		std::mutex key_queue_mutex_;
		std::vector<uint8_t> key_queue_;
};

/*!
	Provides a mapping from idiomatic PC keys to Macintosh keys.
*/
class KeyboardMapper: public KeyboardMachine::MappedMachine::KeyboardMapper {
	uint16_t mapped_key_for_key(Inputs::Keyboard::Key key) override {
		using Key = Inputs::Keyboard::Key;
		switch(key) {
			default: return KeyboardMachine::MappedMachine::KeyNotMapped;

			/*
				See p284 of the Apple Guide to the Macintosh Family Hardware
				for documentation of the mapping below.
			*/

			case Key::BackTick:				return 0x65;
			case Key::k1:					return 0x25;
			case Key::k2:					return 0x27;
			case Key::k3:					return 0x29;
			case Key::k4:					return 0x2b;
			case Key::k5:					return 0x2f;
			case Key::k6:					return 0x2d;
			case Key::k7:					return 0x35;
			case Key::k8:					return 0x39;
			case Key::k9:					return 0x33;
			case Key::k0:					return 0x3b;
			case Key::Hyphen:				return 0x37;
			case Key::Equals:				return 0x31;
			case Key::BackSpace:			return 0x67;

			case Key::Tab:					return 0x61;
			case Key::Q:					return 0x19;
			case Key::W:					return 0x1b;
			case Key::E:					return 0x1d;
			case Key::R:					return 0x1f;
			case Key::T:					return 0x23;
			case Key::Y:					return 0x21;
			case Key::U:					return 0x41;
			case Key::I:					return 0x45;
			case Key::O:					return 0x3f;
			case Key::P:					return 0x47;
			case Key::OpenSquareBracket:	return 0x43;
			case Key::CloseSquareBracket:	return 0x3d;

			case Key::CapsLock:				return 0x73;
			case Key::A:					return 0x01;
			case Key::S:					return 0x03;
			case Key::D:					return 0x05;
			case Key::F:					return 0x07;
			case Key::G:					return 0x0b;
			case Key::H:					return 0x09;
			case Key::J:					return 0x4d;
			case Key::K:					return 0x51;
			case Key::L:					return 0x4b;
			case Key::Semicolon:			return 0x53;
			case Key::Quote:				return 0x4f;
			case Key::Enter:				return 0x49;

			case Key::LeftShift:			return 0x71;
			case Key::Z:					return 0x0d;
			case Key::X:					return 0x0f;
			case Key::C:					return 0x11;
			case Key::V:					return 0x13;
			case Key::B:					return 0x17;
			case Key::N:					return 0x5b;
			case Key::M:					return 0x5d;
			case Key::Comma:				return 0x57;
			case Key::FullStop:				return 0x5f;
			case Key::ForwardSlash:			return 0x59;
			case Key::RightShift:			return 0x71;

			case Key::Left:					return 0x0d;
			case Key::Right:				return 0x05;
			case Key::Up:					return 0x1b;
			case Key::Down:					return 0x11;

			case Key::LeftOption:
			case Key::RightOption:			return 0x75;
			case Key::LeftMeta:
			case Key::RightMeta:			return 0x6f;

			case Key::Space:				return 0x63;
			case Key::BackSlash:			return 0x55;

			/* TODO: the numeric keypad. */
		}
	}

};

}
}

#endif /* Apple_Macintosh_Keyboard_hpp */
