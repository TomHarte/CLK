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

		struct Input {
			/// Defines the broad type of the input.
			enum Type {
				// Half-axis inputs.
				Up, Down, Left, Right,
				// Full-axis inputs.
				Horizontal, Vertical,
				// Fire buttons.
				Fire,
				// Other labelled keys.
				Key
			};
			const Type type;

			enum Precision {
				Analogue, Digital
			};
			const Precision precision;

			/*!
				Holds extra information pertaining to the input.

				@c Type::Key inputs declare the symbol printed on them.

				All other types of input have an associated index, indicating whether they
				are the zeroth, first, second, third, etc of those things. E.g. a joystick
				may have two fire buttons, which will be buttons 0 and 1.
			*/
			union Info {
				struct {
					int index;
				} control;
				struct {
					wchar_t symbol;
				} key;
			};
			Info info;
			// TODO: Find a way to make the above safely const; may mean not using a union.

			Input(Type type, int index = 0, Precision precision = Precision::Digital) :
				type(type),
				precision(precision) {
				info.control.index = index;
			}
			Input(wchar_t symbol) : type(Key), precision(Precision::Digital) {
				info.key.symbol = symbol;
			}

			bool operator == (const Input &rhs) {
				if(rhs.type != type) return false;
				if(rhs.precision != precision) return false;
				if(rhs.type == Key) {
					return rhs.info.key.symbol == info.key.symbol;
				} else {
					return rhs.info.control.index == info.control.index;
				}
			}
		};

		virtual std::vector<Input> get_inputs() = 0;

		// Host interface. Note that the two set_inputs have logic to map
		// between analogue and digital inputs; if you override 
		virtual void set_input(const Input &input, bool is_active) = 0;
		virtual void set_input(const Input &input, float value) {}

		virtual void reset_all_inputs() {
			for(const auto &input: get_inputs()) {
				set_input(input, false);
			}
		}
};

}

#endif /* Joystick_hpp */
