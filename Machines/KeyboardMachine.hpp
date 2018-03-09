//
//  KeyboardMachine.h
//  Clock Signal
//
//  Created by Thomas Harte on 05/11/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#ifndef KeyboardMachine_h
#define KeyboardMachine_h

#include <cstdint>
#include <string>

#include "../Inputs/Keyboard.hpp"

namespace KeyboardMachine {

/*!
	Covers just the actions necessary to communicate keyboard state, as a purely abstract class.
*/
struct KeyActions {
	/*!
		Indicates that the key @c key has been either pressed or released, according to
		the state of @c isPressed.
	*/
	virtual void set_key_state(uint16_t key, bool is_pressed) = 0;

	/*!
		Instructs that all keys should now be treated as released.
	*/
	virtual void clear_all_keys() = 0;
};

/*!
	Describes the full functionality of being an emulated machine with a keyboard: not just being
	able to receive key actions, but being able to vend a generic keyboard and a keyboard mapper.
*/
class Machine: public Inputs::Keyboard::Delegate, public KeyActions {
	public:
		Machine();

		/*!
			Causes the machine to attempt to type the supplied string.

			This is best effort. Success or failure is permitted to be a function of machine and current state.
		*/
		virtual void type_string(const std::string &);

		/*!
			Provides a destination for keyboard input.
		*/
		virtual Inputs::Keyboard &get_keyboard();

		/*!
			A keyboard mapper attempts to provide a physical mapping between host keys and emulated keys.
			See the character mapper for logical mapping.
		*/
		class KeyboardMapper {
			public:
				virtual uint16_t mapped_key_for_key(Inputs::Keyboard::Key key) = 0;
		};

		/// Terminates a key sequence from the character mapper.
		static const uint16_t KeyEndSequence = 0xffff;

		/*!
			Indicates that a key is not mapped (for the keyboard mapper) or that a
			character cannot be typed (for the character mapper).
		*/
		static const uint16_t KeyNotMapped = 0xfffe;

		/*!
			Allows individual machines to provide the mapping between host keys
			as per Inputs::Keyboard and their native scheme.
		*/
		virtual KeyboardMapper *get_keyboard_mapper();

	private:
		void keyboard_did_change_key(Inputs::Keyboard *keyboard, Inputs::Keyboard::Key key, bool is_pressed) override;
		void reset_all_keys(Inputs::Keyboard *keyboard) override;
		Inputs::Keyboard keyboard_;
};

}

#endif /* KeyboardMachine_h */
