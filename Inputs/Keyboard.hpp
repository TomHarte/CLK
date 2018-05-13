//
//  Keyboard.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 10/9/17.
//  Copyright 2017 Thomas Harte. All rights reserved.
//

#ifndef Keyboard_hpp
#define Keyboard_hpp

#include <vector>

namespace Inputs {

/*!
	Provides an intermediate idealised model of a modern-era computer keyboard
	(so, heavily indebted to the current Windows and Mac layouts), allowing a host
	machine to toggle states, while an interested party either observes or polls.
*/
class Keyboard {
	public:
		Keyboard();

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

		// Host interface.
		virtual void set_key_pressed(Key key, char value, bool is_pressed);
		virtual void reset_all_keys();

		// Delegate interface.
		struct Delegate {
			virtual void keyboard_did_change_key(Keyboard *keyboard, Key key, bool is_pressed) = 0;
			virtual void reset_all_keys(Keyboard *keyboard) = 0;
		};
		void set_delegate(Delegate *delegate);
		bool get_key_state(Key key);

	private:
		std::vector<bool> key_states_;
		Delegate *delegate_ = nullptr;
};

}

#endif /* Keyboard_hpp */
