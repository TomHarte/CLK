//
//  Joystick.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 14/10/2017.
//  Copyright 2017 Thomas Harte. All rights reserved.
//

#ifndef Joystick_hpp
#define Joystick_hpp

#include <vector>

namespace Inputs {

/*!
	Provides an intermediate idealised model of a simple joystick, allowing a host
	machine to toggle states, while an interested party either observes or polls.
*/
class Joystick {
	public:
		virtual ~Joystick() {}

		struct DigitalInput {
			enum Type {
				Up, Down, Left, Right, Fire,
				Key
			} type;
			union {
				struct {
					int index;
				} control;
				struct {
					wchar_t symbol;
				} key;
			} info;

			DigitalInput(Type type, int index = 0) : type(type) {
				info.control.index = index;
			}
			DigitalInput(wchar_t symbol) : type(Key) {
				info.key.symbol = symbol;
			}

			bool operator == (const DigitalInput &rhs) {
				if(rhs.type != type) return false;
				if(rhs.type == Key) {
					return rhs.info.key.symbol == info.key.symbol;
				} else {
					return rhs.info.control.index == info.control.index;
				}
			}
		};

		virtual std::vector<DigitalInput> get_inputs() = 0;

		// Host interface.
		virtual void set_digital_input(const DigitalInput &digital_input, bool is_active) = 0;
		virtual void reset_all_inputs() {
			for(const auto &input: get_inputs()) {
				set_digital_input(input, false);
			}
		}
};

}

#endif /* Joystick_hpp */
