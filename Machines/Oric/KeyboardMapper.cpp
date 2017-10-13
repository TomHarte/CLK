//
//  KeyboardMapper.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 10/10/2017.
//  Copyright © 2017 Thomas Harte. All rights reserved.
//

#include "KeyboardMapper.hpp"
#include "Oric.hpp"

using namespace Oric;

uint16_t KeyboardMapper::mapped_key_for_key(Inputs::Keyboard::Key key) {
#define BIND(source, dest)	case Inputs::Keyboard::Key::source:	return Oric::dest
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

		BIND(Left, KeyLeft);	BIND(Right, KeyRight);		BIND(Up, KeyUp);		BIND(Down, KeyDown);

		BIND(Hyphen, KeyMinus);		BIND(Equals, KeyEquals);		BIND(BackSlash, KeyBackSlash);
		BIND(OpenSquareBracket, KeyOpenSquare);	BIND(CloseSquareBracket, KeyCloseSquare);

		BIND(BackSpace, KeyDelete); 	BIND(Delete, KeyDelete);

		BIND(Semicolon, KeySemiColon);	BIND(Quote, KeyQuote);
		BIND(Comma, KeyComma); 		BIND(FullStop, KeyFullStop); 	BIND(ForwardSlash, KeyForwardSlash);

		BIND(Escape, KeyEscape);	BIND(Tab, KeyEscape);
		BIND(CapsLock, KeyControl);	BIND(LeftControl, KeyControl);	BIND(RightControl, KeyControl);
		BIND(LeftOption, KeyFunction);
		BIND(RightOption, KeyFunction);
		BIND(LeftMeta, KeyFunction);
		BIND(RightMeta, KeyFunction);
		BIND(LeftShift, KeyLeftShift);
		BIND(RightShift, KeyRightShift);

		BIND(Space, KeySpace);
		BIND(Enter, KeyReturn);
	}
#undef BIND

	return KeyboardMachine::Machine::KeyNotMapped;
}
