//
//  KeyboardMachine.h
//  Clock Signal
//
//  Created by Thomas Harte on 05/11/2016.
//  Copyright 2016 Thomas Harte. All rights reserved.
//

#ifndef KeyboardMachine_h
#define KeyboardMachine_h

#include <bitset>
#include <cstdint>
#include <string>
#include <set>

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
	virtual void set_key_state(uint16_t key, bool is_pressed) {}

	/*!
		Instructs that all keys should now be treated as released.
	*/
	virtual void clear_all_keys() {}
};

/*!
	Describes an emulated machine which exposes a keyboard and accepts a typed string.
*/
class Machine: public KeyActions {
	public:
		/*!
			Causes the machine to attempt to type the supplied string.

			This is best effort. Success or failure is permitted to be a function of machine and current state.
		*/
		virtual void type_string(const std::string &);

		/*!
			@returns @c true if this machine can type the character @c c as part of a @c type_string; @c false otherwise.
		*/
		virtual bool can_type(char c) { return false; }

		/*!
			Provides a destination for keyboard input.
		*/
		virtual Inputs::Keyboard &get_keyboard() = 0;

		/*!
			Provides a standard bundle of logic for hosts that are able to correlate typed symbols
			with keypresses. Specifically:

			If map_logically is false:

				(i) initially try to set @c key as @c is_pressed;
				(ii) if this machine doesn't map @c key to anything but @c symbol is a printable ASCII character, attempt to @c type_string it.

			If map_logically is true:

				(i) if @c symbol can be typed and this is a key down, @c type_string it;
				(ii) if @c symbol cannot be typed, set @c key as @c is_pressed
		*/
		bool apply_key(Inputs::Keyboard::Key key, char symbol, bool is_pressed, bool map_logically) {
			Inputs::Keyboard &keyboard = get_keyboard();

			if(!map_logically) {
				// Try a regular keypress first, and stop if that works.
				if(keyboard.set_key_pressed(key, symbol, is_pressed)) {
					return true;
				}

				// That having failed, if a symbol has been supplied then try typing it.
				if(is_pressed && symbol && can_type(symbol)) {
					char string[2] = { symbol, 0 };
					type_string(string);
					return true;
				}

				return false;
			} else {
				// Try to type first.
				if(is_pressed && symbol && can_type(symbol)) {
					char string[2] = { symbol, 0 };
					type_string(string);
					return true;
				}

				// That didn't work. Forward as a keypress. As, either:
				//	(i) this is a key down, but doesn't have a symbol, or is an untypeable symbol; or
				//	(ii) this is a key up, which it won't be an issue to miscommunicate.
				return keyboard.set_key_pressed(key, symbol, is_pressed);
			}
		}
};

/*!
	Provides a base class for machines that want to provide a keyboard mapper,
	allowing automatic mapping from keyboard inputs to KeyActions.
*/
class MappedMachine: public Inputs::Keyboard::Delegate, public Machine {
	public:
		MappedMachine(const std::set<Inputs::Keyboard::Key> &essential_modifiers = {});

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

		/*!
			Provides a keyboard that obtains this machine's keyboard mapper, maps
			the key and supplies it via the KeyActions.
		*/
		virtual Inputs::Keyboard &get_keyboard() override;

	private:
		bool keyboard_did_change_key(Inputs::Keyboard *keyboard, Inputs::Keyboard::Key key, bool is_pressed) override;
		void reset_all_keys(Inputs::Keyboard *keyboard) override;
		Inputs::Keyboard keyboard_;
};

}

#endif /* KeyboardMachine_h */
