//
//  KeyboardMachine.h
//  Clock Signal
//
//  Created by Thomas Harte on 05/11/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#ifndef KeyboardMachine_h
#define KeyboardMachine_h

#include "../Inputs/Keyboard.hpp"

namespace KeyboardMachine {

class Machine: public Inputs::Keyboard::Delegate {
	public:
		Machine();
	
		/*!
			Indicates that the key @c key has been either pressed or released, according to
			the state of @c isPressed.
		*/
		virtual void set_key_state(uint16_t key, bool is_pressed) = 0;

		/*!
			Instructs that all keys should now be treated as released.
		*/
		virtual void clear_all_keys() = 0;

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

	protected:
		/*!
			Allows individual machines to provide the mapping between host keys
			as per Inputs::Keyboard and their native scheme.
		*/
		virtual KeyboardMapper &get_keyboard_mapper() = 0;

	private:
		void keyboard_did_change_key(Inputs::Keyboard *keyboard, Inputs::Keyboard::Key key, bool is_pressed);
		void reset_all_keys();
		Inputs::Keyboard keyboard_;
};

}

#endif /* KeyboardMachine_h */
