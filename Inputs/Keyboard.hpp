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
			BackTick, k1, k2, k3, k4, k5, k6, k7, k8, k9, k0, Hyphen, Equals, BackSpace,
			Tab, Q, W, E, R, T, Y, U, I, O, P, OpenSquareBracket, CloseSquareBracket, BackSlash,
			CapsLock, A, S, D, F, G, H, J, K, L, Semicolon, Quote, Hash, Enter,
			LeftShift, Z, X, C, V, B, N, M, Comma, FullStop, ForwardSlash, RightShift,
			LeftControl, LeftOption, LeftMeta, Space, RightMeta, RightOption, RightControl,
			Left, Right, Up, Down,
			Insert, Home, PageUp, Delete, End, PageDown,
			NumLock, KeyPadSlash, KeyPadAsterisk, KeyPadDelete,
			KeyPad7, KeyPad8, KeyPad9, KeyPadPlus,
			KeyPad4, KeyPad5, KeyPad6, KeyPadMinus,
			KeyPad1, KeyPad2, KeyPad3, KeyPadEnter,
			KeyPad0, KeyPadDecimalPoint, KeyPadEquals,
			Help
		};

		/// Constructs a Keyboard that declares itself to observe all keys.
		Keyboard();

		/// Constructs a Keyboard that declares itself to observe only members of @c observed_keys.
		Keyboard(const std::set<Key> &observed_keys);

		// Host interface.
		virtual void set_key_pressed(Key key, char value, bool is_pressed);
		virtual void reset_all_keys();

		/// @returns a set of all Keys that this keyboard responds to.
		virtual const std::set<Key> &observed_keys();

		/*
			@returns @c true if this keyboard, on its original machine, looked
			like a complete keyboard — i.e. if a user would expect this keyboard
			to be the only thing a real keyboard maps to.
		*/
		virtual bool is_exclusive();

		// Delegate interface.
		struct Delegate {
			virtual void keyboard_did_change_key(Keyboard *keyboard, Key key, bool is_pressed) = 0;
			virtual void reset_all_keys(Keyboard *keyboard) = 0;
		};
		void set_delegate(Delegate *delegate);
		bool get_key_state(Key key);

	private:
		std::set<Key> observed_keys_;
		std::vector<bool> key_states_;
		Delegate *delegate_ = nullptr;
		bool is_exclusive_ = true;
};

}

#endif /* Inputs_Keyboard_hpp */
