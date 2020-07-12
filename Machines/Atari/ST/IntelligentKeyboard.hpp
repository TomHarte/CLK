//
//  IntelligentKeyboard.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 02/11/2019.
//  Copyright © 2019 Thomas Harte. All rights reserved.
//

#ifndef IntelligentKeyboard_hpp
#define IntelligentKeyboard_hpp

#include "../../../ClockReceiver/ClockingHintSource.hpp"
#include "../../../Components/Serial/Line.hpp"
#include "../../KeyboardMachine.hpp"

#include "../../../Inputs/Joystick.hpp"
#include "../../../Inputs/Mouse.hpp"

#include <atomic>
#include <mutex>
#include <memory>
#include <vector>

namespace Atari {
namespace ST {

enum class Key: uint16_t {
	Escape = 1,
	k1, k2, k3, k4, k5, k6, k7, k8, k9, k0, Hyphen, Equals, Backspace,
	Tab, Q, W, E, R, T, Y, U, I, O, P, OpenSquareBracket, CloseSquareBracket, Return,
	Control, A, S, D, F, G, H, J, K, L, Semicolon, Quote, BackTick,
	LeftShift, Backslash, Z, X, C, V, B, N, M, Comma, FullStop, ForwardSlash, RightShift,
	/* 0x37 is unused. */
	Alt = 0x38, Space, CapsLock, F1, F2, F3, F4, F5, F6, F7, F8, F9, F10,
	/* Various gaps follow. */
	Home = 0x47, Up,
	KeypadMinus = 0x4a, Left,
	Right = 0x4d, KeypadPlus,
	Down = 0x50,
	Insert = 0x52, Delete,
	ISO = 0x60, Undo, Help, KeypadOpenBracket, KeypadCloseBracket, KeypadDivide, KeypadMultiply,
	Keypad7, Keypad8, Keypad9, Keypad4, Keypad5, Keypad6, Keypad1, Keypad2, Keypad3, Keypad0, KeypadDecimalPoint,
	KeypadEnter,
	Joystick1Button = 0x74,	// These keycodes are used only in joystick keycode mode.
	Joystick2Button = 0x75,
};
static_assert(uint16_t(Key::RightShift) == 0x36, "RightShift should have key code 0x36; check intermediate entries");
static_assert(uint16_t(Key::F10) == 0x44, "F10 should have key code 0x44; check intermediate entries");
static_assert(uint16_t(Key::KeypadEnter) == 0x72, "KeypadEnter should have key code 0x72; check intermediate entries");

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
		ClockingHint::Preference preferred_clocking() const final;
		void run_for(HalfCycles duration);

		void set_key_state(Key key, bool is_pressed);
		class KeyboardMapper: public MachineTypes::MappedKeyboardMachine::KeyboardMapper {
			uint16_t mapped_key_for_key(Inputs::Keyboard::Key key) const final;
		};

		const std::vector<std::unique_ptr<Inputs::Joystick>> &get_joysticks() {
			return joysticks_;
		}

	private:
		// MARK: - Key queue.
		std::mutex key_queue_mutex_;
		std::vector<uint8_t> key_queue_;

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
			Relative, Absolute, Disabled
		} mouse_mode_ = MouseMode::Relative;

		// Absolute positioning state.
		int mouse_range_[2] = {320, 200};
		int mouse_scale_[2] = {1, 1};
		int mouse_position_[2] = {0, 0};
		int mouse_y_multiplier_ = 1;

		// Relative positioning state.
		int posted_button_state_ = 0;
		int mouse_threshold_[2] = {1, 1};
		void post_relative_mouse_event(int x, int y);

		// Received mouse state.
		std::atomic<int> mouse_movement_[2]{0, 0};
		std::atomic<int> mouse_button_state_{0};
		std::atomic<int> mouse_button_events_{0};

		// MARK: - Joystick.
		void disable_joysticks();
		void set_joystick_event_mode();
		void set_joystick_interrogation_mode();
		void set_joystick_monitoring_mode(uint8_t rate);
		void set_joystick_fire_button_monitoring_mode();
		struct VelocityThreshold {
			uint8_t threshold;
			uint8_t prior_rate;
			uint8_t post_rate;
		};
		void set_joystick_keycode_mode(VelocityThreshold horizontal, VelocityThreshold vertical);
		void interrogate_joysticks();

		void clear_joystick_events();

		enum class JoystickMode {
			Disabled, Event, Interrogation, KeyCode
		} joystick_mode_ = JoystickMode::Event;

		class Joystick: public Inputs::ConcreteJoystick {
			public:
				Joystick() :
					ConcreteJoystick({
						Input(Input::Up),
						Input(Input::Down),
						Input(Input::Left),
						Input(Input::Right),
						Input(Input::Fire, 0),
					}) {}

				void did_set_input(const Input &input, bool is_active) final {
					uint8_t mask = 0;
					switch(input.type) {
						default: return;
						case Input::Up:		mask = 0x01;	break;
						case Input::Down:	mask = 0x02;	break;
						case Input::Left:	mask = 0x04;	break;
						case Input::Right:	mask = 0x08;	break;
						case Input::Fire:	mask = 0x80;	break;
					}

					if(is_active) state_ |= mask; else state_ &= ~mask;
				}

				uint8_t get_state() {
					returned_state_ = state_;
					return state_;
				}

				bool has_event() {
					return returned_state_ != state_;
				}

				uint8_t event_mask() {
					return returned_state_ ^ state_;
				}

			private:
				uint8_t state_ = 0x00;
				uint8_t returned_state_ = 0x00;
		};
		std::vector<std::unique_ptr<Inputs::Joystick>> joysticks_;
};

}
}

#endif /* IntelligentKeyboard_hpp */
