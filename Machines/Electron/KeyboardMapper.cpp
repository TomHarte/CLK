//
//  KeyboardMapper.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 10/10/2017.
//  Copyright © 2017 Thomas Harte. All rights reserved.
//

#include "KeyboardMapper.hpp"
#include "Electron.hpp"

using namespace Electron;

uint16_t KeyboardMapper::mapped_key_for_key(Inputs::Keyboard::Key key) {
#define BIND(source, dest)	case Inputs::Keyboard::Key::source:	return Electron::Key::dest
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

		BIND(Comma, KeyComma);
		BIND(FullStop, KeyFullStop);
		BIND(ForwardSlash, KeySlash);
		BIND(Semicolon, KeySemiColon);
		BIND(Quote, KeyColon);

		BIND(Escape, KeyEscape);
		BIND(Equals, KeyBreak);
		BIND(F12, KeyBreak);

		BIND(Left, KeyLeft);	BIND(Right, KeyRight);		BIND(Up, KeyUp);		BIND(Down, KeyDown);

		BIND(Tab, KeyFunc);				BIND(LeftOption, KeyFunc);		BIND(RightOption, KeyFunc);
		BIND(LeftMeta, KeyFunc);		BIND(RightMeta, KeyFunc);
		BIND(CapsLock, KeyControl);		BIND(LeftControl, KeyControl);	BIND(RightControl, KeyControl);
		BIND(LeftShift, KeyShift);		BIND(RightShift, KeyShift);

		BIND(Hyphen, KeyMinus);
		BIND(Delete, KeyDelete);
		BIND(Enter, KeyReturn);			BIND(KeyPadEnter, KeyReturn);

		BIND(KeyPad0, Key0);		BIND(KeyPad1, Key1);		BIND(KeyPad2, Key2);		BIND(KeyPad3, Key3);		BIND(KeyPad4, Key4);
		BIND(KeyPad5, Key5);		BIND(KeyPad6, Key6);		BIND(KeyPad7, Key7);		BIND(KeyPad8, Key8);		BIND(KeyPad9, Key9);

		BIND(KeyPadMinus, KeyMinus);			BIND(KeyPadPlus, KeyColon);

		BIND(Space, KeySpace);
	}
#undef BIND
}
