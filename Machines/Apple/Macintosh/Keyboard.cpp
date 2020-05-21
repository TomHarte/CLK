//
//  Keyboard.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 02/08/2019.
//  Copyright © 2019 Thomas Harte. All rights reserved.
//

#include "Keyboard.hpp"

using namespace Apple::Macintosh;

uint16_t KeyboardMapper::mapped_key_for_key(Inputs::Keyboard::Key key) const {
	using Key = Inputs::Keyboard::Key;
	using MacKey = Apple::Macintosh::Key;
	switch(key) {
		default: return MachineTypes::MappedKeyboardMachine::KeyNotMapped;

#define Bind(x, y) case Key::x: return uint16_t(y)

		Bind(BackTick, MacKey::BackTick);
		Bind(k1, MacKey::k1);	Bind(k2, MacKey::k2);	Bind(k3, MacKey::k3);
		Bind(k4, MacKey::k4);	Bind(k5, MacKey::k5);	Bind(k6, MacKey::k6);
		Bind(k7, MacKey::k7);	Bind(k8, MacKey::k8);	Bind(k9, MacKey::k9);
		Bind(k0, MacKey::k0);
		Bind(Hyphen, MacKey::Hyphen);
		Bind(Equals, MacKey::Equals);
		Bind(Backspace, MacKey::Backspace);

		Bind(Tab, MacKey::Tab);
		Bind(Q, MacKey::Q);		Bind(W, MacKey::W);		Bind(E, MacKey::E);		Bind(R, MacKey::R);
		Bind(T, MacKey::T);		Bind(Y, MacKey::Y);		Bind(U, MacKey::U);		Bind(I, MacKey::I);
		Bind(O, MacKey::O);		Bind(P, MacKey::P);
		Bind(OpenSquareBracket, MacKey::OpenSquareBracket);
		Bind(CloseSquareBracket, MacKey::CloseSquareBracket);

		Bind(CapsLock, MacKey::CapsLock);
		Bind(A, MacKey::A);		Bind(S, MacKey::S);		Bind(D, MacKey::D);		Bind(F, MacKey::F);
		Bind(G, MacKey::G);		Bind(H, MacKey::H);		Bind(J, MacKey::J);		Bind(K, MacKey::K);
		Bind(L, MacKey::L);
		Bind(Semicolon, MacKey::Semicolon);
		Bind(Quote, MacKey::Quote);
		Bind(Enter, MacKey::Return);

		Bind(LeftShift, MacKey::Shift);
		Bind(Z, MacKey::Z);		Bind(X, MacKey::X);		Bind(C, MacKey::C);		Bind(V, MacKey::V);
		Bind(B, MacKey::B);		Bind(N, MacKey::N);		Bind(M, MacKey::M);
		Bind(Comma, MacKey::Comma);
		Bind(FullStop, MacKey::FullStop);
		Bind(ForwardSlash, MacKey::ForwardSlash);
		Bind(RightShift, MacKey::Shift);

		Bind(Left, MacKey::Left);
		Bind(Right, MacKey::Right);
		Bind(Up, MacKey::Up);
		Bind(Down, MacKey::Down);

		Bind(LeftOption, MacKey::Option);
		Bind(RightOption, MacKey::Option);
		Bind(LeftMeta, MacKey::Command);
		Bind(RightMeta, MacKey::Command);

		Bind(Space, MacKey::Space);
		Bind(Backslash, MacKey::Backslash);

		Bind(KeypadDelete, MacKey::KeypadDelete);
		Bind(KeypadEquals, MacKey::KeypadEquals);
		Bind(KeypadSlash, MacKey::KeypadSlash);
		Bind(KeypadAsterisk, MacKey::KeypadAsterisk);
		Bind(KeypadMinus, MacKey::KeypadMinus);
		Bind(KeypadPlus, MacKey::KeypadPlus);
		Bind(KeypadEnter, MacKey::KeypadEnter);
		Bind(KeypadDecimalPoint, MacKey::KeypadDecimalPoint);

		Bind(Keypad9, MacKey::Keypad9);
		Bind(Keypad8, MacKey::Keypad8);
		Bind(Keypad7, MacKey::Keypad7);
		Bind(Keypad6, MacKey::Keypad6);
		Bind(Keypad5, MacKey::Keypad5);
		Bind(Keypad4, MacKey::Keypad4);
		Bind(Keypad3, MacKey::Keypad3);
		Bind(Keypad2, MacKey::Keypad2);
		Bind(Keypad1, MacKey::Keypad1);
		Bind(Keypad0, MacKey::Keypad0);

#undef Bind
	}
}
