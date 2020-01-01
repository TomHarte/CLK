//
//  IntelligentKeyboard.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 02/11/2019.
//  Copyright © 2019 Thomas Harte. All rights reserved.
//

#include "IntelligentKeyboard.hpp"

#include <algorithm>

#define LOG_PREFIX "[IKYB] "
#include "../../../Outputs/Log.hpp"

using namespace Atari::ST;

IntelligentKeyboard::IntelligentKeyboard(Serial::Line &input, Serial::Line &output) : output_line_(output) {
	input.set_read_delegate(this, Storage::Time(2, 15625));
	output_line_.set_writer_clock_rate(15625);

	// Add two joysticks into the mix.
	joysticks_.emplace_back(new Joystick);
	joysticks_.emplace_back(new Joystick);

	mouse_button_state_ = 0;
	mouse_button_events_ = 0;
	mouse_movement_[0] = 0;
	mouse_movement_[1] = 0;
}

bool IntelligentKeyboard::serial_line_did_produce_bit(Serial::Line *, int bit) {
	// Shift.
	command_ = (command_ >> 1) | (bit << 9);

	// If that's 10 bits, decode a byte and stop.
	bit_count_ = (bit_count_ + 1) % 10;
	if(!bit_count_) {
		dispatch_command(uint8_t(command_ >> 1));
		command_ = 0;
		return false;
	}

	// Continue.
	return true;
}

ClockingHint::Preference IntelligentKeyboard::preferred_clocking() {
	return output_line_.transmission_data_time_remaining().as_integral() ? ClockingHint::Preference::RealTime : ClockingHint::Preference::None;
}

void IntelligentKeyboard::run_for(HalfCycles duration) {
	// Take this opportunity to check for joystick, mouse and keyboard events,
	// which will have been received asynchronously.
	const int captured_movement[2] = { mouse_movement_[0].load(), mouse_movement_[1].load() };
	switch(mouse_mode_) {
		case MouseMode::Relative: {
			const int captured_button_state = mouse_button_state_;
			if(
				(posted_button_state_ != captured_button_state) ||
				(abs(captured_movement[0]) >= mouse_threshold_[0]) ||
				(abs(captured_movement[1]) >= mouse_threshold_[1]) ) {
				mouse_movement_[0] -= captured_movement[0];
				mouse_movement_[1] -= captured_movement[1];

				post_relative_mouse_event(captured_movement[0], captured_movement[1] * mouse_y_multiplier_);
			}
		} break;

		case MouseMode::Absolute: {
			const int scaled_movement[2] = { captured_movement[0] / mouse_scale_[0], captured_movement[1] / mouse_scale_[1] };
			mouse_position_[0] += scaled_movement[0];
			mouse_position_[1] += mouse_y_multiplier_ * scaled_movement[1];

			// Clamp to range.
			mouse_position_[0] = std::min(std::max(mouse_position_[0], 0), mouse_range_[0]);
			mouse_position_[1] = std::min(std::max(mouse_position_[1], 0), mouse_range_[1]);

			mouse_movement_[0] -= scaled_movement[0] * mouse_scale_[0];
			mouse_movement_[1] -= scaled_movement[1] * mouse_scale_[1];
		} break;

		case MouseMode::Disabled:
			mouse_movement_[0] = 0;
			mouse_movement_[1] = 0;
		break;
	}

	// Forward key changes; implicit assumption here: mutexs are cheap while there's
	// negligible contention.
	{
		std::lock_guard<decltype(key_queue_mutex_)> guard(key_queue_mutex_);
		for(uint8_t key: key_queue_) {
			output_bytes({key});
		}
		key_queue_.clear();
	}

	// Check for joystick changes; slight complexity here: the joystick that the emulated
	// machine advertises as joystick 1 is mapped to the Atari ST's joystick 2, so as to
	// maintain both the normal emulation expections that the first joystick is the primary
	// one and the Atari ST's convention that the main joystick is in port 2.
	for(size_t c = 0; c < 2; ++c) {
		const auto joystick = static_cast<Joystick *>(joysticks_[c ^ 1].get());
		if(joystick->has_event()) {
			output_bytes({
				uint8_t(0xfe | c),
				joystick->get_state()
			});
		}
	}

	output_line_.advance_writer(duration);
}

void IntelligentKeyboard::output_bytes(std::initializer_list<uint8_t> values) {
	// Wrap the value in a start and stop bit, and send it on its way.
	for(auto value : values) {
		output_line_.write(2, 10, 0x200 | (value << 1));
	}
	update_clocking_observer();
}

void IntelligentKeyboard::dispatch_command(uint8_t command) {
	// Enqueue for parsing.
	command_sequence_.push_back(command);

	// For each possible command, check that the proper number of bytes are present.
	// If not, exit. If so, perform and drop out of the switch.
	switch(command_sequence_.front()) {
		default:
			printf("Unrecognised IKBD command %02x\n", command);
		break;

		case 0x80:
			/*
				Reset: 0x80 0x01.
				"Any byte following an 0x80 command byte other than 0x01 is ignored (and causes the 0x80 to be ignored)."
			*/
			if(command_sequence_.size() != 2) return;
			if(command_sequence_[1] == 0x01) {
				reset();
			}
		break;

		case 0x07:
			if(command_sequence_.size() != 2) return;
			set_mouse_button_actions(command_sequence_[1]);
		break;

		case 0x08:
			set_relative_mouse_position_reporting();
		break;

		case 0x09:
			if(command_sequence_.size() != 5) return;
			set_absolute_mouse_position_reporting(
				uint16_t((command_sequence_[1] << 8) | command_sequence_[2]),
				uint16_t((command_sequence_[3] << 8) | command_sequence_[4])
			);
		break;

		case 0x0a:
			if(command_sequence_.size() != 3) return;
			set_mouse_keycode_reporting(command_sequence_[1], command_sequence_[2]);
		break;

		case 0x0b:
			if(command_sequence_.size() != 3) return;
			set_mouse_threshold(command_sequence_[1], command_sequence_[2]);
		break;

		case 0x0c:
			if(command_sequence_.size() != 3) return;
			set_mouse_scale(command_sequence_[1], command_sequence_[2]);
		break;

		case 0x0d:
			interrogate_mouse_position();
		break;

		case 0x0e:
			if(command_sequence_.size() != 6) return;
			/* command_sequence_[1] has no defined meaning. */
			set_mouse_position(
				uint16_t((command_sequence_[2] << 8) | command_sequence_[3]),
				uint16_t((command_sequence_[4] << 8) | command_sequence_[5])
			);
		break;

		case 0x0f:	set_mouse_y_upward();		break;
		case 0x10:	set_mouse_y_downward();		break;
		case 0x11:	resume();					break;
		case 0x12:	disable_mouse();			break;
		case 0x13:	pause();					break;

		/* Joystick commands. */
		case 0x14:	set_joystick_event_mode();					break;
		case 0x15:	set_joystick_interrogation_mode();			break;
		case 0x16:	interrogate_joysticks();					break;
		case 0x17:
			if(command_sequence_.size() != 2) return;
			set_joystick_monitoring_mode(command_sequence_[1]);
		break;
		case 0x18:	set_joystick_fire_button_monitoring_mode();	break;
		case 0x19: {
			if(command_sequence_.size() != 7) return;

			VelocityThreshold horizontal, vertical;
			horizontal.threshold = command_sequence_[1];
			horizontal.prior_rate = command_sequence_[3];
			horizontal.post_rate = command_sequence_[5];

			vertical.threshold = command_sequence_[2];
			vertical.prior_rate = command_sequence_[4];
			vertical.post_rate = command_sequence_[6];

			set_joystick_keycode_mode(horizontal, vertical);
		} break;
		case 0x1a:	disable_joysticks();						break;
	}

	// There was no premature exit, so a complete command sequence must have been satisfied.
	command_sequence_.clear();
}

void IntelligentKeyboard::reset() {
	// Reset should perform a self test, lasting at most 200ms, then post 0xf0.
	// Following that it should look for any keys that currently seem to be pressed.
	// Those are considered stuck and a break code is generated for them.
	output_bytes({0xf0});
}

void IntelligentKeyboard::resume() {
	LOG("Unimplemented: resume");
}

void IntelligentKeyboard::pause() {
	LOG("Unimplemented: pause");
}

void IntelligentKeyboard::disable_mouse() {
	mouse_mode_ = MouseMode::Disabled;
}

void IntelligentKeyboard::set_relative_mouse_position_reporting() {
	mouse_mode_ = MouseMode::Relative;
}

void IntelligentKeyboard::set_absolute_mouse_position_reporting(uint16_t max_x, uint16_t max_y) {
	mouse_mode_ = MouseMode::Absolute;
	mouse_range_[0] = int(max_x);
	mouse_range_[1] = int(max_y);
}

void IntelligentKeyboard::set_mouse_position(uint16_t x, uint16_t y) {
	mouse_position_[0] = std::min(int(x), mouse_range_[0]);
	mouse_position_[1] = std::min(int(y), mouse_range_[1]);
}

void IntelligentKeyboard::set_mouse_keycode_reporting(uint8_t delta_x, uint8_t delta_y) {
	LOG("Unimplemented: set mouse keycode reporting");
}

void IntelligentKeyboard::set_mouse_threshold(uint8_t x, uint8_t y) {
	mouse_threshold_[0] = x;
	mouse_threshold_[1] = y;
}

void IntelligentKeyboard::set_mouse_scale(uint8_t x, uint8_t y) {
	mouse_scale_[0] = x;
	mouse_scale_[1] = y;
}

void IntelligentKeyboard::set_mouse_y_downward() {
	mouse_y_multiplier_ = 1;
}

void IntelligentKeyboard::set_mouse_y_upward() {
	mouse_y_multiplier_ = -1;
}

void IntelligentKeyboard::set_mouse_button_actions(uint8_t actions) {
	LOG("Unimplemented: set mouse button actions");
}

void IntelligentKeyboard::interrogate_mouse_position() {
	const int captured_mouse_button_events_ = mouse_button_events_;
	mouse_button_events_ &= ~captured_mouse_button_events_;
	output_bytes({
		0xf7,									// Beginning of mouse response.
		uint8_t(captured_mouse_button_events_),	// 0000dcba; a = right button down since last interrogation, b = right button up since, c/d = left button.
		uint8_t(mouse_position_[0] >> 8),		// x position: MSB, LSB
		uint8_t(mouse_position_[0] & 0xff),
		uint8_t(mouse_position_[1] >> 8),		// y position: MSB, LSB
		uint8_t(mouse_position_[1] & 0xff)
	});
}

void IntelligentKeyboard::post_relative_mouse_event(int x, int y) {
	posted_button_state_ = mouse_button_state_;

	// Break up the motion to impart, if it's too large.
	do {
		int stepped_motion[2] = {
			(x >= -128 && x < 127) ? x : (x > 0 ? 127 : -128),
			(y >= -128 && y < 127) ? y : (y > 0 ? 127 : -128),
		};

		output_bytes({
			uint8_t(0xf8 | posted_button_state_),	// Command code is a function of button state.
			uint8_t(stepped_motion[0]),
			uint8_t(stepped_motion[1]),
		});

		x -= stepped_motion[0];
		y -= stepped_motion[1];
	} while(x || y);
}

// MARK: - Keyboard Input
void IntelligentKeyboard::set_key_state(Key key, bool is_pressed) {
	std::lock_guard<decltype(key_queue_mutex_)> guard(key_queue_mutex_);
	if(is_pressed) {
		key_queue_.push_back(uint8_t(key));
	} else {
		key_queue_.push_back(0x80 | uint8_t(key));
	}
}

uint16_t IntelligentKeyboard::KeyboardMapper::mapped_key_for_key(Inputs::Keyboard::Key key) {
	using Key = Inputs::Keyboard::Key;
	using STKey = Atari::ST::Key;
	switch(key) {
		default: return KeyboardMachine::MappedMachine::KeyNotMapped;

#define Bind(x, y) case Key::x: return uint16_t(STKey::y)
#define QBind(x) case Key::x: return uint16_t(STKey::x)

		QBind(k1);	QBind(k2);	QBind(k3);	QBind(k4);	QBind(k5);	QBind(k6);	QBind(k7);	QBind(k8);	QBind(k9);	QBind(k0);
		QBind(Q);	QBind(W);	QBind(E);	QBind(R);	QBind(T);	QBind(Y);	QBind(U);	QBind(I);	QBind(O);	QBind(P);
		QBind(A);	QBind(S);	QBind(D);	QBind(F);	QBind(G);	QBind(H);	QBind(J);	QBind(K);	QBind(L);
		QBind(Z);	QBind(X);	QBind(C);	QBind(V);	QBind(B);	QBind(N);	QBind(M);

		QBind(Left);	QBind(Right);	QBind(Up);	QBind(Down);

		QBind(BackTick);	QBind(Tab);
		QBind(Hyphen);		QBind(Equals);
		QBind(Backspace);	QBind(Delete);
		QBind(OpenSquareBracket);
		QBind(CloseSquareBracket);
		QBind(CapsLock);
		QBind(Semicolon);
		QBind(Quote);
		Bind(Enter, Return);
		QBind(LeftShift);
		QBind(RightShift);
		QBind(Escape);
		QBind(Home);
		QBind(Insert);

		Bind(F12, Help);	Bind(F11, Help);
		Bind(PageUp, Undo);
		Bind(PageDown, ISO);

		Bind(Comma, Comma);
		Bind(FullStop, FullStop);
		Bind(ForwardSlash, ForwardSlash);

		Bind(LeftOption, Alt);
		Bind(RightOption, Alt);
		Bind(LeftControl, Control);
		Bind(RightControl, Control);
		QBind(Space);
		QBind(Backslash);

		QBind(Keypad0);	QBind(Keypad1);	QBind(Keypad2);	QBind(Keypad3);	QBind(Keypad4);
		QBind(Keypad5);	QBind(Keypad6);	QBind(Keypad7);	QBind(Keypad8);	QBind(Keypad9);
		QBind(KeypadMinus);
		QBind(KeypadPlus);
		QBind(KeypadDecimalPoint);
		QBind(KeypadEnter);

		QBind(F1);	QBind(F2);	QBind(F3);	QBind(F4);	QBind(F5);
		QBind(F6);	QBind(F7);	QBind(F8);	QBind(F9);	QBind(F10);

#undef QBind
#undef Bind
	}
}

// MARK: - Mouse Input

void IntelligentKeyboard::move(int x, int y) {
	mouse_movement_[0] += x;
	mouse_movement_[1] += y;
}

int IntelligentKeyboard::get_number_of_buttons() {
	return 2;
}

void IntelligentKeyboard::set_button_pressed(int index, bool is_pressed) {
	index ^= 1;		// The primary button is b1; the secondary is b0.

	const auto mask = 1 << index;
	const auto event_mask = 1 << (index << 1);
	if(is_pressed) {
		mouse_button_state_ |= mask;
		mouse_button_events_ |= event_mask;
	} else {
		mouse_button_state_ &= ~mask;
		mouse_button_events_ |= event_mask << 1;
	}
}

void IntelligentKeyboard::reset_all_buttons() {
	mouse_button_state_ = 0;
}

// MARK: - Joystick Output
void IntelligentKeyboard::disable_joysticks() {
	joystick_mode_ = JoystickMode::Disabled;
}

void IntelligentKeyboard::set_joystick_event_mode() {
	joystick_mode_ = JoystickMode::Event;
}

void IntelligentKeyboard::set_joystick_interrogation_mode() {
	joystick_mode_ = JoystickMode::Interrogation;
}

void IntelligentKeyboard::interrogate_joysticks() {
	const auto joystick1 = static_cast<Joystick *>(joysticks_[0].get());
	const auto joystick2 = static_cast<Joystick *>(joysticks_[1].get());

	output_bytes({
		0xfd,
		joystick2->get_state(),
		joystick1->get_state()
	});
}

void IntelligentKeyboard::set_joystick_monitoring_mode(uint8_t rate) {
	LOG("Unimplemented: joystick monitoring mode");
}

void IntelligentKeyboard::set_joystick_fire_button_monitoring_mode() {
	LOG("Unimplemented: joystick fire button monitoring mode");
}

void IntelligentKeyboard::set_joystick_keycode_mode(VelocityThreshold horizontal, VelocityThreshold vertical) {
	LOG("Unimplemented: joystick keycode mode");
}
