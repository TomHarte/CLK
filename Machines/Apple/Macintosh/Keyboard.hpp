//
//  Keyboard.h
//  Clock Signal
//
//  Created by Thomas Harte on 08/05/2019.
//  Copyright © 2019 Thomas Harte. All rights reserved.
//

#ifndef Keyboard_hpp
#define Keyboard_hpp

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
				case Mode::AwaitingEndOfCommand:
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

	private:

		int perform_command(int command) {
			switch(command) {
				case 0x10:	// Inquiry.
				break;

				case 0x14:	// Instant.
				break;

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
};

}
}

#endif /* Keyboard_h */
