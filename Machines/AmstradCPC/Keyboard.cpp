//
//  Keyboard.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 10/10/2017.
//  Copyright 2017 Thomas Harte. All rights reserved.
//

#include "Keyboard.hpp"

using namespace AmstradCPC;

uint16_t KeyboardMapper::mapped_key_for_key(const Inputs::Keyboard::Key key) const {
#define BIND(source, dest)	case Inputs::Keyboard::Key::source:	return dest
	switch(key) {
		default: break;

		BIND(BackTick, KeyCopy);

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

		BIND(Hyphen, KeyMinus);		BIND(Equals, KeyCaret);		BIND(Backspace, KeyDelete);
		BIND(Tab, KeyTab);

		BIND(OpenSquareBracket, KeyAt);
		BIND(CloseSquareBracket, KeyLeftSquareBracket);
		BIND(Backslash, KeyBackSlash);

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

		BIND(Keypad0, KeyF0);
		BIND(Keypad1, KeyF1);		BIND(Keypad2, KeyF2);		BIND(Keypad3, KeyF3);
		BIND(Keypad4, KeyF4);		BIND(Keypad5, KeyF5);		BIND(Keypad6, KeyF6);
		BIND(Keypad7, KeyF7);		BIND(Keypad8, KeyF8);		BIND(Keypad9, KeyF9);
		BIND(KeypadPlus, KeySemicolon);
		BIND(KeypadMinus, KeyMinus);

		BIND(KeypadEnter, KeyEnter);
		BIND(KeypadDecimalPoint, KeyFullStop);
		BIND(KeypadEquals, KeyMinus);
		BIND(KeypadSlash, KeyForwardSlash);
		BIND(KeypadAsterisk, KeyColon);
		BIND(KeypadDelete, KeyDelete);
	}
#undef BIND
	return MachineTypes::MappedKeyboardMachine::KeyNotMapped;
}

namespace {
const std::unordered_map<wchar_t, const std::vector<uint16_t>> sequences = {
	{L'\b', {KeyDelete}},
	{L'\n', {KeyReturn}},	{L'\r', {KeyReturn}},
	{L' ', {KeySpace}},

	{L'!', {KeyShift, Key1}},
	{L'"', {KeyShift, Key2}},
	{L'#', {KeyShift, Key3}},
	{L'$', {KeyShift, Key4}},
	{L'%', {KeyShift, Key5}},
	{L'&', {KeyShift, Key6}},
	{L'\'', {KeyShift, Key7}},
	{L'(', {KeyShift, Key8}},
	{L')', {KeyShift, Key9}},
	{L'_', {KeyShift, Key0}},

	{L'=', {KeyShift, KeyMinus}},		{L'-', {KeyMinus}},
	{L'£', {KeyShift, KeyCaret}},		{L'↑', {KeyCaret}},

	{L'|', {KeyShift, KeyAt}},					{L'@', {KeyAt}},
	{L'{', {KeyShift, KeyLeftSquareBracket}},	{L'[', {KeyLeftSquareBracket}},

	{L'*', {KeyShift, KeyColon}},				{L':', {KeyColon}},
	{L'+', {KeyShift, KeySemicolon}},			{L';', {KeySemicolon}},
	{L'}', {KeyShift, KeyRightSquareBracket}},	{L']', {KeyRightSquareBracket}},

	{L'<', {KeyShift, KeyComma}},				{L',', {KeyComma}},
	{L'>', {KeyShift, KeyFullStop}},			{L'.', {KeyFullStop}},
	{L'?', {KeyShift, KeyForwardSlash}},		{L'/', {KeyForwardSlash}},
	{L'\\', {KeyBackSlash}},

	{L'0', {Key0}},	{L'1', {Key1}},	{L'2', {Key2}},	{L'3', {Key3}},	{L'4', {Key4}},
	{L'5', {Key5}},	{L'6', {Key6}},	{L'7', {Key7}},	{L'8', {Key8}},	{L'9', {Key9}},

	{L'A', {KeyShift, KeyA}},	{L'B', {KeyShift, KeyB}},	{L'C', {KeyShift, KeyC}},
	{L'D', {KeyShift, KeyD}},	{L'E', {KeyShift, KeyE}},	{L'F', {KeyShift, KeyF}},
	{L'G', {KeyShift, KeyG}},	{L'H', {KeyShift, KeyH}},	{L'I', {KeyShift, KeyI}},
	{L'J', {KeyShift, KeyJ}},	{L'K', {KeyShift, KeyK}},	{L'L', {KeyShift, KeyL}},
	{L'M', {KeyShift, KeyM}},	{L'N', {KeyShift, KeyN}},	{L'O', {KeyShift, KeyO}},
	{L'P', {KeyShift, KeyP}},	{L'Q', {KeyShift, KeyQ}},	{L'R', {KeyShift, KeyR}},
	{L'S', {KeyShift, KeyS}},	{L'T', {KeyShift, KeyT}},	{L'U', {KeyShift, KeyU}},
	{L'V', {KeyShift, KeyV}},	{L'W', {KeyShift, KeyW}},	{L'X', {KeyShift, KeyX}},
	{L'Y', {KeyShift, KeyY}},	{L'Z', {KeyShift, KeyZ}},

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

bool CharacterMapper::needs_pause_after_key(const uint16_t key) const {
	return key != KeyControl && key != KeyShift;
}
