//
//  Keyboard.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 10/10/2017.
//  Copyright 2017 Thomas Harte. All rights reserved.
//

#include "Keyboard.hpp"

using namespace Commodore::Vic20;

uint16_t KeyboardMapper::mapped_key_for_key(Inputs::Keyboard::Key key) const {
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

		BIND(BackTick, KeyLeftArrow);
		BIND(Hyphen, KeyPlus);
		BIND(Equals, KeyDash);
		BIND(F11, KeyGBP);
		BIND(F12, KeyHome);

		BIND(Tab, KeyControl);
		BIND(OpenSquareBracket, KeyAt);
		BIND(CloseSquareBracket, KeyAsterisk);

		BIND(Backslash, KeyRestore);
		BIND(Hash, KeyUpArrow);
		BIND(F10, KeyUpArrow);

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
		BIND(Backspace, KeyDelete);

		BIND(Escape, KeyRunStop);
		BIND(F1, KeyF1);
		BIND(F3, KeyF3);
		BIND(F5, KeyF5);
		BIND(F7, KeyF7);

		// Mappings to virtual keys.
		BIND(Left, KeyLeft);
		BIND(Up, KeyUp);
		BIND(F2, KeyF2);
		BIND(F4, KeyF4);
		BIND(F6, KeyF6);
		BIND(F8, KeyF8);
	}
#undef BIND
	return MachineTypes::MappedKeyboardMachine::KeyNotMapped;
}

namespace {
const std::unordered_map<wchar_t, const std::vector<uint16_t>> sequences = {
	{L'\b', {KeyDelete}},
	{L'\n', {KeyReturn}},	{L'\r', {KeyReturn}},
	{L' ', {KeySpace}},

	{L'!', {KeyLShift, Key1}},
	{L'"', {KeyLShift, Key2}},
	{L'#', {KeyLShift, Key3}},
	{L'$', {KeyLShift, Key4}},
	{L'%', {KeyLShift, Key5}},
	{L'&', {KeyLShift, Key6}},
	{L'\'', {KeyLShift, Key7}},
	{L'(', {KeyLShift, Key8}},
	{L')', {KeyLShift, Key9}},

	{L'0', {Key0}},	{L'1', {Key1}},	{L'2', {Key2}},	{L'3', {Key3}},	{L'4', {Key4}},
	{L'5', {Key5}},	{L'6', {Key6}},	{L'7', {Key7}},	{L'8', {Key8}},	{L'9', {Key9}},
	{L'+', {KeyPlus}},
	{L'-', {KeyDash}},
	{L'£', {KeyGBP}},

	{L'@', {KeyAt}},
	{L'*', {KeyAsterisk}},
	{L'↑', {KeyUpArrow}},

	{L'[', {KeyLShift, KeyColon}},		{L':', {KeyColon}},
	{L']', {KeyLShift, KeySemicolon}},	{L';', {KeySemicolon}},
	{L'=', {KeyEquals}},

	{L'<', {KeyLShift, KeyComma}},		{L',', {KeyComma}},
	{L'>', {KeyLShift, KeyFullStop}},	{L'.', {KeyFullStop}},
	{L'?', {KeyLShift, KeySlash}},		{L'/', {KeySlash}},

	{L'a', {KeyA}},	{L'b', {KeyB}},	{L'c', {KeyC}},
	{L'd', {KeyD}},	{L'e', {KeyE}},	{L'f', {KeyF}},
	{L'g', {KeyG}},	{L'h', {KeyH}},	{L'i', {KeyI}},
	{L'j', {KeyJ}},	{L'k', {KeyK}},	{L'l', {KeyL}},
	{L'm', {KeyM}},	{L'n', {KeyN}},	{L'o', {KeyO}},
	{L'p', {KeyP}},	{L'q', {KeyQ}},	{L'r', {KeyR}},
	{L's', {KeyS}},	{L't', {KeyT}},	{L'u', {KeyU}},
	{L'v', {KeyV}},	{L'w', {KeyW}},	{L'x', {KeyX}},
	{L'y', {KeyY}},	{L'z', {KeyZ}},
};
}

const std::vector<uint16_t> *CharacterMapper::sequence_for_character(const wchar_t character) const {
	return lookup_sequence(sequences, character);
}
