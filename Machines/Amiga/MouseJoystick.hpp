//
//  MouseJoystick.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 26/11/2021.
//  Copyright © 2021 Thomas Harte. All rights reserved.
//

#ifndef MouseJoystick_hpp
#define MouseJoystick_hpp

#include <array>
#include <atomic>

#include "../../Inputs/Joystick.hpp"
#include "../../Inputs/Mouse.hpp"

namespace Amiga {

struct MouseJoystickInput {
	virtual uint16_t get_position() = 0;
	virtual uint8_t get_cia_button() const = 0;
};

class Mouse: public Inputs::Mouse, public MouseJoystickInput {
	public:
		uint16_t get_position() final;
		uint8_t get_cia_button() const final;

	private:
		int get_number_of_buttons() final;
		void set_button_pressed(int, bool) final;
		void reset_all_buttons() final;
		void move(int, int) final;

		uint8_t declared_position_[2]{};
		uint8_t cia_state_ = 0xff;
		std::array<std::atomic<int>, 2> position_{};
};

class Joystick: public Inputs::ConcreteJoystick, public MouseJoystickInput {
	public:
		Joystick();

		uint16_t get_position() final;
		uint8_t get_cia_button() const final;

	private:
		void did_set_input(const Input &input, bool is_active) final;

		bool inputs_[Joystick::Input::Type::Max]{};
		uint16_t position_ = 0;
};

}

#endif /* MouseJoystick_hpp */
