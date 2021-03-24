//
//  Keyboard.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 29/11/2017.
//  Copyright 2017 Thomas Harte. All rights reserved.
//

#include "Keyboard.hpp"

uint16_t MSX::KeyboardMapper::mapped_key_for_key(Inputs::Keyboard::Key key) const {
#define BIND(source, dest)	case Inputs::Keyboard::Key::source:	return MSX::Key::dest
	switch(key) {
		BIND(k0, Key0);		BIND(k1, Key1);		BIND(k2, Key2);		BIND(k3, Key3);		BIND(k4, Key4);
		BIND(k5, Key5);		BIND(k6, Key6);		BIND(k7, Key7);		BIND(k8, Key8);		BIND(k9, Key9);
		BIND(Q, KeyQ);		BIND(W, KeyW);		BIND(E, KeyE);		BIND(R, KeyR);		BIND(T, KeyT);
		BIND(Y, KeyY);		BIND(U, KeyU);		BIND(I, KeyI);		BIND(O, KeyO);		BIND(P, KeyP);
		BIND(A, KeyA);		BIND(S, KeyS);		BIND(D, KeyD);		BIND(F, KeyF);		BIND(G, KeyG);
		BIND(H, KeyH);		BIND(J, KeyJ);		BIND(K, KeyK);		BIND(L, KeyL);
		BIND(Z, KeyZ);		BIND(X, KeyX);		BIND(C, KeyC);		BIND(V, KeyV);
		BIND(B, KeyB);		BIND(N, KeyN);		BIND(M, KeyM);

		BIND(F1, KeyF1);	BIND(F2, KeyF2);	BIND(F3, KeyF3);	BIND(F4, KeyF4);	BIND(F5, KeyF5);

		BIND(F12, KeyStop);
		BIND(F10, KeyDelete);		BIND(F9, KeyInsert);		BIND(F8, KeyHome);
		BIND(Delete, KeyDelete);	BIND(Insert, KeyInsert);	BIND(Home, KeyHome);

		BIND(Escape, KeyEscape);
		BIND(Tab, KeyTab);			BIND(CapsLock, KeyCaps);

		BIND(LeftControl, KeyControl);	BIND(RightControl, KeyControl);
		BIND(LeftShift, KeyShift);		BIND(RightShift, KeyShift);
		BIND(LeftMeta, KeyCode);		BIND(RightMeta, KeyGraph);
		BIND(LeftOption, KeyCode);		BIND(RightOption, KeySelect);

		BIND(Semicolon, KeySemicolon);
		BIND(Quote, KeyQuote);
		BIND(OpenSquareBracket, KeyLeftSquareBracket);
		BIND(CloseSquareBracket, KeyRightSquareBracket);
		BIND(Hyphen, KeyMinus);
		BIND(Equals, KeyEquals);
		BIND(Left, KeyLeft);
		BIND(Right, KeyRight);
		BIND(Up, KeyUp);
		BIND(Down, KeyDown);
		BIND(FullStop, KeyFullStop);
		BIND(Comma, KeyComma);
		BIND(ForwardSlash, KeyForwardSlash);
		BIND(Backslash, KeyBackSlash);
		BIND(BackTick, KeyGrave);

		BIND(Enter, KeyEnter);
		BIND(Space, KeySpace);
		BIND(Backspace, KeyBackspace);

		default: break;
	}
#undef BIND
	return MachineTypes::MappedKeyboardMachine::KeyNotMapped;
}
