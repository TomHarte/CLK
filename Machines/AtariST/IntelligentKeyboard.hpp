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

#include "../../Inputs/Mouse.hpp"

#include <atomic>

namespace Atari {
namespace ST {

/*!
	A receiver for the Atari ST's "intelligent keyboard" commands, which actually cover
	keyboard input and output and mouse handling.
*/
class IntelligentKeyboard:
	public Serial::Line::ReadDelegate,
	public ClockingHint::Source,
	public Inputs::Mouse {
	public:
		IntelligentKeyboard(Serial::Line &input, Serial::Line &output);
		ClockingHint::Preference preferred_clocking() final;
		void run_for(HalfCycles duration);

	private:
		// MARK: - Serial line state.
		int bit_count_ = 0;
		int command_ = 0;
		Serial::Line &output_line_;

		void output_bytes(std::initializer_list<uint8_t> value);
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

		// Inputs::Mouse.
		void move(int x, int y) final;
		int get_number_of_buttons() final;
		void set_button_pressed(int index, bool is_pressed) final;
		void reset_all_buttons() final;

		enum class MouseMode {
			Relative, Absolute
		} mouse_mode_ = MouseMode::Relative;

		// Absolute positioning state.
		int mouse_range_[2] = {0, 0};
		int mouse_scale_[2] = {0, 0};

		// Relative positioning state.
		int posted_button_state_ = 0;
		int mouse_threshold_[2] = {1, 1};
		void post_relative_mouse_event(int x, int y);

		// Received mouse state.
		std::atomic<int> mouse_movement_[2];
		std::atomic<int> mouse_button_state_;

		// MARK: - Joystick.
		void disable_joysticks();
};

}
}

#endif /* IntelligentKeyboard_hpp */
