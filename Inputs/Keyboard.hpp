//
//  Keyboard.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 10/9/17.
//  Copyright 2017 Thomas Harte. All rights reserved.
//

#ifndef Inputs_Keyboard_hpp
#define Inputs_Keyboard_hpp

#include <vector>
#include <set>

namespace Inputs {

/*!
	Provides an intermediate idealised model of a modern-era computer keyboard
	(so, heavily indebted to the current Windows and Mac layouts), allowing a host
	machine to toggle states, while an interested party either observes or polls.
*/
class Keyboard {
	public:
		enum class Key {
			Escape, F1, F2, F3, F4, F5, F6, F7, F8, F9, F10, F11, F12, PrintScreen, ScrollLock, Pause,
			BackTick, k1, k2, k3, k4, k5, k6, k7, k8, k9, k0, Hyphen, Equals, Backspace,
			Tab, Q, W, E, R, T, Y, U, I, O, P, OpenSquareBracket, CloseSquareBracket, Backslash,
			CapsLock, A, S, D, F, G, H, J, K, L, Semicolon, Quote, Hash, Enter,
			LeftShift, Z, X, C, V, B, N, M, Comma, FullStop, ForwardSlash, RightShift,
			LeftControl, LeftOption, LeftMeta, Space, RightMeta, RightOption, RightControl,
			Left, Right, Up, Down,
			Insert, Home, PageUp, Delete, End, PageDown,
			NumLock, KeypadSlash, KeypadAsterisk, KeypadDelete,
			Keypad7, Keypad8, Keypad9, KeypadPlus,
			Keypad4, Keypad5, Keypad6, KeypadMinus,
			Keypad1, Keypad2, Keypad3, KeypadEnter,
			Keypad0, KeypadDecimalPoint, KeypadEquals,
			Help,

			Max = Help
		};

		/// Constructs a Keyboard that declares itself to observe all keys.
		Keyboard(const std::set<Key> &essential_modifiers = {});

		/// Constructs a Keyboard that declares itself to observe only members of @c observed_keys.
		Keyboard(const std::set<Key> &observed_keys, const std::set<Key> &essential_modifiers);

		// Host interface.

		/// @returns @c true if the key press affects the machine; @c false otherwise.
		virtual bool set_key_pressed(Key key, char value, bool is_pressed);
		virtual void reset_all_keys();

		/// @returns a set of all Keys that this keyboard responds to.
		virtual const std::set<Key> &observed_keys() const;

		/// @returns the list of modifiers that this keyboard considers 'essential' (i.e. both mapped and highly used).
		virtual const std::set<Inputs::Keyboard::Key> &get_essential_modifiers() const;

		/*!
			@returns @c true if this keyboard, on its original machine, looked
			like a complete keyboard — i.e. if a user would expect this keyboard
			to be the only thing a real keyboard maps to.

			So this would be true of something like the Amstrad CPC, which has a full
			keyboard, but it would be false of something like the Sega Master System
			which has some buttons that you'd expect an emulator to map to its host
			keyboard but which does not offer a full keyboard.
		*/
		virtual bool is_exclusive() const;

		// Delegate interface.
		struct Delegate {
			virtual bool keyboard_did_change_key(Keyboard *keyboard, Key key, bool is_pressed) = 0;
			virtual void reset_all_keys(Keyboard *keyboard) = 0;
		};
		void set_delegate(Delegate *delegate);
		bool get_key_state(Key key) const;

	private:
		std::set<Key> observed_keys_;
		const std::set<Key> essential_modifiers_;
		const bool is_exclusive_ = true;

		std::vector<bool> key_states_;
		Delegate *delegate_ = nullptr;
};

}

#endif /* Inputs_Keyboard_hpp */
