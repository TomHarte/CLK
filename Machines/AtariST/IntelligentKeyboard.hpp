//
//  IntelligentKeyboard.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 02/11/2019.
//  Copyright © 2019 Thomas Harte. All rights reserved.
//

#ifndef IntelligentKeyboard_hpp
#define IntelligentKeyboard_hpp

#include "../../ClockReceiver/ClockingHintSource.hpp"
#include "../../Components/SerialPort/SerialPort.hpp"

namespace Atari {
namespace ST {

/*!
	A receiver for the Atari ST's "intelligent keyboard" commands, which actually cover
	keyboard input and output and mouse handling.
*/
class IntelligentKeyboard:
	public Serial::Line::ReadDelegate,
	public ClockingHint::Source {
	public:
		IntelligentKeyboard(Serial::Line &input, Serial::Line &output);
		ClockingHint::Preference preferred_clocking() final;
		void run_for(HalfCycles duration);

	private:
		// MARK: - Serial line state.
		int bit_count_ = 0;
		int command_ = 0;
		Serial::Line &output_line_;

		void output_byte(uint8_t value);
		bool serial_line_did_produce_bit(Serial::Line *, int bit) final;

		// MARK: - Command dispatch.
		std::vector<uint8_t> command_sequence_;
		void dispatch_command(uint8_t command);

		// MARK: - Flow control.
		void reset();
		void resume();
		void pause();

		// MARK: - Mouse.
		void disable_mouse();
		void set_relative_mouse_position_reporting();
		void set_absolute_mouse_position_reporting(uint16_t max_x, uint16_t max_y);
		void set_mouse_position(uint16_t x, uint16_t y);
		void set_mouse_keycode_reporting(uint8_t delta_x, uint8_t delta_y);
		void set_mouse_threshold(uint8_t x, uint8_t y);
		void set_mouse_scale(uint8_t x, uint8_t y);
		void set_mouse_y_downward();
		void set_mouse_y_upward();
		void set_mouse_button_actions(uint8_t actions);
		void interrogate_mouse_position();

		// MARK: - Joystick.
		void disable_joysticks();
};

}
}

#endif /* IntelligentKeyboard_hpp */
