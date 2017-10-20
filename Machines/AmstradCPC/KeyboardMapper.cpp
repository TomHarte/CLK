//
//  KeyboardMapper.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 10/10/2017.
//  Copyright © 2017 Thomas Harte. All rights reserved.
//

#include "KeyboardMapper.hpp"
#include "AmstradCPC.hpp"

using namespace AmstradCPC;

uint16_t KeyboardMapper::mapped_key_for_key(Inputs::Keyboard::Key key) {
#define BIND(source, dest)	case Inputs::Keyboard::Key::source:	return dest
	switch(key) {
		default: return KeyCopy;

		BIND(k0, Key0);		BIND(k1, Key1);		BIND(k2, Key2);		BIND(k3, Key3);		BIND(k4, Key4);
		BIND(k5, Key5);		BIND(k6, Key6);		BIND(k7, Key7);		BIND(k8, Key8);		BIND(k9, Key9);
		BIND(Q, KeyQ);		BIND(W, KeyW);		BIND(E, KeyE);		BIND(R, KeyR);		BIND(T, KeyT);
		BIND(Y, KeyY);		BIND(U, KeyU);		BIND(I, KeyI);		BIND(O, KeyO);		BIND(P, KeyP);
		BIND(A, KeyA);		BIND(S, KeyS);		BIND(D, KeyD);		BIND(F, KeyF);		BIND(G, KeyG);
		BIND(H, KeyH);		BIND(J, KeyJ);		BIND(K, KeyK);		BIND(L, KeyL);
		BIND(Z, KeyZ);		BIND(X, KeyX);		BIND(C, KeyC);		BIND(V, KeyV);
		BIND(B, KeyB);		BIND(N, KeyN);		BIND(M, KeyM);

		BIND(Escape, KeyEscape);
		BIND(F1, KeyF1);	BIND(F2, KeyF2);	BIND(F3, KeyF3);	BIND(F4, KeyF4);	BIND(F5, KeyF5);
		BIND(F6, KeyF6);	BIND(F7, KeyF7);	BIND(F8, KeyF8);	BIND(F9, KeyF9);	BIND(F10, KeyF0);

		BIND(F11, KeyRightSquareBracket);
		BIND(F12, KeyClear);

		BIND(Hyphen, KeyMinus);		BIND(Equals, KeyCaret);		BIND(BackSpace, KeyDelete);
		BIND(Tab, KeyTab);

		BIND(OpenSquareBracket, KeyAt);
		BIND(CloseSquareBracket, KeyLeftSquareBracket);
		BIND(BackSlash, KeyBackSlash);

		BIND(CapsLock, KeyCapsLock);
		BIND(Semicolon, KeyColon);
		BIND(Quote, KeySemicolon);
		BIND(Hash, KeyRightSquareBracket);
		BIND(Enter, KeyReturn);

		BIND(LeftShift, KeyShift);
		BIND(Comma, KeyComma);
		BIND(FullStop, KeyFullStop);
		BIND(ForwardSlash, KeyForwardSlash);
		BIND(RightShift, KeyShift);

		BIND(LeftControl, KeyControl);	BIND(LeftOption, KeyControl);	BIND(LeftMeta, KeyControl);
		BIND(Space, KeySpace);
		BIND(RightMeta, KeyControl);	BIND(RightOption, KeyControl);	BIND(RightControl, KeyControl);

		BIND(Left, KeyLeft);	BIND(Right, KeyRight);
		BIND(Up, KeyUp);		BIND(Down, KeyDown);

		BIND(KeyPad0, KeyF0);
		BIND(KeyPad1, KeyF1);		BIND(KeyPad2, KeyF2);		BIND(KeyPad3, KeyF3);
		BIND(KeyPad4, KeyF4);		BIND(KeyPad5, KeyF5);		BIND(KeyPad6, KeyF6);
		BIND(KeyPad7, KeyF7);		BIND(KeyPad8, KeyF8);		BIND(KeyPad9, KeyF9);
		BIND(KeyPadPlus, KeySemicolon);
		BIND(KeyPadMinus, KeyMinus);

		BIND(KeyPadEnter, KeyEnter);
		BIND(KeyPadDecimalPoint, KeyFullStop);
		BIND(KeyPadEquals, KeyMinus);
		BIND(KeyPadSlash, KeyForwardSlash);
		BIND(KeyPadAsterisk, KeyColon);
		BIND(KeyPadDelete, KeyDelete);
	}
#undef BIND
}
