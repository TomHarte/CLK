//
//  Keyboard.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 10/10/2017.
//  Copyright 2017 Thomas Harte. All rights reserved.
//

#include "Keyboard.hpp"

using namespace Electron;

uint16_t KeyboardMapper::mapped_key_for_key(const Inputs::Keyboard::Key key) const {
#define BIND(source, dest)	case Inputs::Keyboard::Key::source:	return Electron::Key::dest
	switch(key) {
		default: break;

		BIND(BackTick, KeyCopy);
		BIND(Backslash, KeyCopy);

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
		BIND(F12, KeyBreak);

		BIND(Left, KeyLeft);	BIND(Right, KeyRight);		BIND(Up, KeyUp);		BIND(Down, KeyDown);

		BIND(Tab, KeyFunc);				BIND(LeftOption, KeyFunc);		BIND(RightOption, KeyFunc);
		BIND(LeftMeta, KeyFunc);		BIND(RightMeta, KeyFunc);
		BIND(CapsLock, KeyControl);		BIND(LeftControl, KeyControl);	BIND(RightControl, KeyControl);
		BIND(LeftShift, KeyShift);		BIND(RightShift, KeyShift);

		BIND(Hyphen, KeyMinus);
		BIND(Delete, KeyDelete);		BIND(Backspace, KeyDelete);
		BIND(Enter, KeyReturn);			BIND(KeypadEnter, KeyReturn);

		BIND(Keypad0, Key0);		BIND(Keypad1, Key1);		BIND(Keypad2, Key2);		BIND(Keypad3, Key3);		BIND(Keypad4, Key4);
		BIND(Keypad5, Key5);		BIND(Keypad6, Key6);		BIND(Keypad7, Key7);		BIND(Keypad8, Key8);		BIND(Keypad9, Key9);

		BIND(KeypadMinus, KeyMinus);			BIND(KeypadPlus, KeyColon);

		BIND(Space, KeySpace);

		// Virtual mappings.
		BIND(F1, KeyF1);
		BIND(F2, KeyF2);
		BIND(F3, KeyF3);
		BIND(F4, KeyF4);
		BIND(F5, KeyF5);
		BIND(F6, KeyF6);
		BIND(F7, KeyF7);
		BIND(F8, KeyF8);
		BIND(F9, KeyF9);
		BIND(F10, KeyF0);
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
	{L'@', {KeyShift, Key0}},

	{L'-', {KeyMinus}},
	{L'=', {KeyShift, KeyMinus}},

	{L'^', {KeyShift, KeyLeft}},
	{L'~', {KeyControl, KeyLeft}},
	{L'|', {KeyShift, KeyRight}},
	{L'\\', {KeyControl, KeyRight}},
	{L'£', {KeyShift, KeyUp}},
	{L'{', {KeyControl, KeyUp}},
	{L'_', {KeyShift, KeyDown}},
	{L'}', {KeyControl, KeyDown}},
	{L'[', {KeyShift, KeyCopy}},
	{L']', {KeyControl, KeyCopy}},

	{L'+', {KeyShift, KeySemiColon}},	{L';', {KeySemiColon}},
	{L'*', {KeyShift, KeyColon}},		{L':', {KeyColon}},

	{L'<', {KeyShift, KeyComma}},		{L',', {KeyComma}},
	{L'>', {KeyShift, KeyFullStop}},	{L'.', {KeyFullStop}},
	{L'?', {KeyShift, KeySlash}},		{L'/', {KeySlash}},

	{L'0', {Key0}},	{L'1', {Key1}},	{L'2', {Key2}},	{L'3', {Key3}},	{L'4', {Key4}},
	{L'5', {Key5}},	{L'6', {Key6}},	{L'7', {Key7}},	{L'8', {Key8}},	{L'9', {Key9}},

	{L'a', {KeyShift, KeyA}},	{L'b', {KeyShift, KeyB}},	{L'c', {KeyShift, KeyC}},
	{L'd', {KeyShift, KeyD}},	{L'e', {KeyShift, KeyE}},	{L'f', {KeyShift, KeyF}},
	{L'g', {KeyShift, KeyG}},	{L'h', {KeyShift, KeyH}},	{L'i', {KeyShift, KeyI}},
	{L'j', {KeyShift, KeyJ}},	{L'k', {KeyShift, KeyK}},	{L'l', {KeyShift, KeyL}},
	{L'm', {KeyShift, KeyM}},	{L'n', {KeyShift, KeyN}},	{L'o', {KeyShift, KeyO}},
	{L'p', {KeyShift, KeyP}},	{L'q', {KeyShift, KeyQ}},	{L'r', {KeyShift, KeyR}},
	{L's', {KeyShift, KeyS}},	{L't', {KeyShift, KeyT}},	{L'u', {KeyShift, KeyU}},
	{L'v', {KeyShift, KeyV}},	{L'w', {KeyShift, KeyW}},	{L'x', {KeyShift, KeyX}},
	{L'y', {KeyShift, KeyY}},	{L'z', {KeyShift, KeyZ}},

	{L'A', {KeyA}},	{L'B', {KeyB}},	{L'C', {KeyC}},
	{L'D', {KeyD}},	{L'E', {KeyE}},	{L'F', {KeyF}},
	{L'G', {KeyG}},	{L'H', {KeyH}},	{L'I', {KeyI}},
	{L'J', {KeyJ}},	{L'K', {KeyK}},	{L'L', {KeyL}},
	{L'M', {KeyM}},	{L'N', {KeyN}},	{L'O', {KeyO}},
	{L'P', {KeyP}},	{L'Q', {KeyQ}},	{L'R', {KeyR}},
	{L'S', {KeyS}},	{L'T', {KeyT}},	{L'U', {KeyU}},
	{L'V', {KeyV}},	{L'W', {KeyW}},	{L'X', {KeyX}},
	{L'Y', {KeyY}},	{L'Z', {KeyZ}},
};
}

std::span<const uint16_t> CharacterMapper::sequence_for_character(const wchar_t character) const {
	return lookup_sequence(sequences, character);
}

bool CharacterMapper::needs_pause_after_key(const uint16_t key) const {
	return !is_modifier(Key(key));
}
