//
//  KeyboardMapper.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 10/10/2017.
//  Copyright © 2017 Thomas Harte. All rights reserved.
//

#include "KeyboardMapper.hpp"
#include "Vic20.hpp"

using namespace Commodore::Vic20;

uint16_t KeyboardMapper::mapped_key_for_key(Inputs::Keyboard::Key key) {
#define BIND(source, dest)	case Inputs::Keyboard::Key::source:	return Commodore::Vic20::dest
	switch(key) {
		default: break;

		BIND(k0, Key0);		BIND(k1, Key1);		BIND(k2, Key2);		BIND(k3, Key3);		BIND(k4, Key4);
		BIND(k5, Key5);		BIND(k6, Key6);		BIND(k7, Key7);		BIND(k8, Key8);		BIND(k9, Key9);
		BIND(Q, KeyQ);		BIND(W, KeyW);		BIND(E, KeyE);		BIND(R, KeyR);		BIND(T, KeyT);
		BIND(Y, KeyY);		BIND(U, KeyU);		BIND(I, KeyI);		BIND(O, KeyO);		BIND(P, KeyP);
		BIND(A, KeyA);		BIND(S, KeyS);		BIND(D, KeyD);		BIND(F, KeyF);		BIND(G, KeyG);
		BIND(H, KeyH);		BIND(J, KeyJ);		BIND(K, KeyK);		BIND(L, KeyL);
		BIND(Z, KeyZ);		BIND(X, KeyX);		BIND(C, KeyC);		BIND(V, KeyV);
		BIND(B, KeyB);		BIND(N, KeyN);		BIND(M, KeyM);

		BIND(BackTick, KeyLeft);
		BIND(Hyphen, KeyPlus);
		BIND(Equals, KeyDash);
		BIND(F11, KeyGBP);
		BIND(F12, KeyHome);

		BIND(Tab, KeyControl);
		BIND(OpenSquareBracket, KeyAt);
		BIND(CloseSquareBracket, KeyAsterisk);

		BIND(BackSlash, KeyRestore);
		BIND(Hash, KeyUp);
		BIND(F10, KeyUp);

		BIND(Semicolon, KeyColon);
		BIND(Quote, KeySemicolon);
		BIND(F9, KeyEquals);

		BIND(LeftMeta, KeyCBM);
		BIND(LeftOption, KeyCBM);
		BIND(RightOption, KeyCBM);
		BIND(RightMeta, KeyCBM);

		BIND(LeftShift, KeyLShift);
		BIND(RightShift, KeyRShift);

		BIND(Comma, KeyComma);
		BIND(FullStop, KeyFullStop);
		BIND(ForwardSlash, KeySlash);

		BIND(Right, KeyRight);
		BIND(Down, KeyDown);

		BIND(Enter, KeyReturn);
		BIND(Space, KeySpace);
		BIND(BackSpace, KeyDelete);
	}
#undef BIND
	return KeyboardMachine::Machine::KeyNotMapped;
}
