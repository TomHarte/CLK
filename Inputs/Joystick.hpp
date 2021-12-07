//
//  Joystick.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 14/10/2017.
//  Copyright 2017 Thomas Harte. All rights reserved.
//

#ifndef Joystick_hpp
#define Joystick_hpp

#include <cstddef>
#include <vector>

namespace Inputs {

/*!
	Provides an intermediate idealised model of a simple joystick, allowing a host
	machine to toggle states, while an interested party either observes or polls.
*/
class Joystick {
	public:
		virtual ~Joystick() {}

		/*!
			Defines a single input, any individually-measured thing — a fire button or
			other digital control, an analogue axis, or a button with a symbol on it.
		*/
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
				Key,

				// The maximum value this enum can contain.
				Max = Key
			};
			const Type type;

			bool is_digital_axis() const {
				return type < Type::Horizontal;
			}
			bool is_analogue_axis() const {
				return type >= Type::Horizontal && type < Type::Fire;
			}
			bool is_axis() const {
				return type < Type::Fire;
			}
			bool is_button() const {
				return type >= Type::Fire;
			}

			enum Precision {
				Analogue, Digital
			};
			Precision precision() const {
				return is_analogue_axis() ? Precision::Analogue : Precision::Digital;
			}

			/*!
				Holds extra information pertaining to the input.

				@c Type::Key inputs declare the symbol printed on them.

				All other types of input have an associated index, indicating whether they
				are the zeroth, first, second, third, etc of those things. E.g. a joystick
				may have two fire buttons, which will be buttons 0 and 1.
			*/
			union Info {
				struct {
					size_t index;
				} control;
				struct {
					wchar_t symbol;
				} key;
			};
			Info info;
			// TODO: Find a way to make the above safely const; may mean not using a union.

			Input(Type type, size_t index = 0) :
				type(type) {
				info.control.index = index;
			}
			Input(wchar_t symbol) : type(Key) {
				info.key.symbol = symbol;
			}

			bool operator == (const Input &rhs) {
				if(rhs.type != type) return false;
				if(rhs.type == Key) {
					return rhs.info.key.symbol == info.key.symbol;
				} else {
					return rhs.info.control.index == info.control.index;
				}
			}
		};

		/// @returns The list of all inputs defined on this joystick.
		virtual const std::vector<Input> &get_inputs() = 0;

		/*!
			Sets the digital value of @c input. This may have direct effect or
			influence an analogue value; e.g. if the caller declares that ::Left is
			active but this joystick has only an analogue horizontal axis, this will
			cause a change to that analogue value.
		*/
		virtual void set_input(const Input &input, bool is_active) = 0;

		/*!
			Sets the analogue value of @c input. If the input is actually digital,
			or if there is a digital input with a corresponding meaning (e.g. ::Left
			versus the horizontal axis), this may cause a digital input to be set.

			@c value should be in the range [0.0, 1.0].
		*/
		virtual void set_input(const Input &input, float value) = 0;

		/*!
			Sets all inputs to their resting state.
		*/
		virtual void reset_all_inputs() {
			for(const auto &input: get_inputs()) {
				if(input.precision() == Input::Precision::Digital)
					set_input(input, false);
				else
					set_input(input, 0.5f);
			}
		}

		/*!
			Gets the number of input fire buttons.

			This is cached by default, but it's virtual so overridable.
		*/
		virtual int get_number_of_fire_buttons() {
			if(number_of_buttons_ >= 0) return number_of_buttons_;

			number_of_buttons_ = 0;
			for(const auto &input: get_inputs()) {
				if(input.type == Input::Type::Fire) ++number_of_buttons_;
			}
			return number_of_buttons_;
		}

	private:
		int number_of_buttons_ = -1;
};

/*!
	ConcreteJoystick is the class that it's expected most machines will actually subclass;
	it accepts a set of Inputs at construction and thereby is able to provide the
	promised analogue <-> digital mapping of Joystick.
*/
class ConcreteJoystick: public Joystick {
	public:
		ConcreteJoystick(const std::vector<Input> &inputs) : inputs_(inputs) {
			// Size and populate stick_types_, which is used for digital <-> analogue conversion.
			for(const auto &input: inputs_) {
				const bool is_digital_axis = input.is_digital_axis();
				const bool is_analogue_axis = input.is_analogue_axis();
				if(is_digital_axis || is_analogue_axis) {
					const size_t required_size = size_t(input.info.control.index+1);
					if(stick_types_.size() < required_size) {
						stick_types_.resize(required_size);
					}
					stick_types_[size_t(input.info.control.index)] = is_digital_axis ? StickType::Digital : StickType::Analogue;
				}
			}
		}

		const std::vector<Input> &get_inputs() final {
			return inputs_;
		}

		void set_input(const Input &input, bool is_active) final {
			// If this is a digital setting to a digital property, just pass it along.
			if(input.is_button() || stick_types_[input.info.control.index] == StickType::Digital) {
				did_set_input(input, is_active);
				return;
			}

			// Otherwise this is logically to an analogue axis; for now just use some
			// convenient hard-coded values. TODO: make these a function of time.
			using Type = Joystick::Input::Type;
			switch(input.type) {
				default:			did_set_input(input, is_active ? 1.0f : 0.0f);													break;
				case Type::Left:	did_set_input(Input(Type::Horizontal, input.info.control.index), is_active ? 0.1f : 0.5f);		break;
				case Type::Right:	did_set_input(Input(Type::Horizontal, input.info.control.index), is_active ? 0.9f : 0.5f);		break;
				case Type::Up:		did_set_input(Input(Type::Vertical, input.info.control.index), is_active ? 0.1f : 0.5f);		break;
				case Type::Down:	did_set_input(Input(Type::Vertical, input.info.control.index), is_active ? 0.9f : 0.5f);		break;
			}
		}

		void set_input(const Input &input, float value) final {
			// If this is an analogue setting to an analogue property, just pass it along.
			if(!input.is_button() && stick_types_[input.info.control.index] == StickType::Analogue) {
				did_set_input(input, value);
				return;
			}

			// Otherwise apply a threshold test to convert to digital, with remapping from axes to digital inputs.
			using Type = Joystick::Input::Type;
			switch(input.type) {
				default:			did_set_input(input, value > 0.5f);											break;
				case Type::Horizontal:
					did_set_input(Input(Type::Left, input.info.control.index), value <= 0.25f);
					did_set_input(Input(Type::Right, input.info.control.index), value >= 0.75f);
				break;
				case Type::Vertical:
					did_set_input(Input(Type::Up, input.info.control.index), value <= 0.25f);
					did_set_input(Input(Type::Down, input.info.control.index), value >= 0.75f);
				break;
			}
		}

	protected:
		virtual void did_set_input([[maybe_unused]] const Input &input, [[maybe_unused]] float value) {}
		virtual void did_set_input([[maybe_unused]] const Input &input, [[maybe_unused]] bool value) {}

	private:
		const std::vector<Input> inputs_;

		enum class StickType {
			Digital,
			Analogue
		};
		std::vector<StickType> stick_types_;
};

}

#endif /* Joystick_hpp */
