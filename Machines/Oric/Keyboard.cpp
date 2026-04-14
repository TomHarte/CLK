//
//  Keyboard.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 10/10/2017.
//  Copyright 2017 Thomas Harte. All rights reserved.
//

#include "Keyboard.hpp"

using namespace Oric;

uint16_t KeyboardMapper::mapped_key_for_key(Inputs::Keyboard::Key key) const {
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

		BIND(Hyphen, KeyMinus);		BIND(Equals, KeyEquals);		BIND(Backslash, KeyBackSlash);
		BIND(OpenSquareBracket, KeyOpenSquare);	BIND(CloseSquareBracket, KeyCloseSquare);

		BIND(Backspace, KeyDelete);	BIND(Delete, KeyDelete);

		BIND(Semicolon, KeySemiColon);	BIND(Quote, KeyQuote);
		BIND(Comma, KeyComma);			BIND(FullStop, KeyFullStop);	BIND(ForwardSlash, KeyForwardSlash);

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

		BIND(F12, KeyNMI);
		BIND(F1, KeyJasminReset);
	}
#undef BIND

	return MachineTypes::MappedKeyboardMachine::KeyNotMapped;
}

namespace {
const std::unordered_map<wchar_t, const std::vector<uint16_t>> sequences = {
	{L'\e', {KeyEscape}},
	{L'\b', {KeyDelete}},
	{L'\n', {KeyReturn}},	{L'\r', {KeyReturn}},
	{L' ', {KeySpace}},

	{L'!', {KeyLeftShift, Key1}},
	{L'@', {KeyLeftShift, Key2}},
	{L'#', {KeyLeftShift, Key3}},
	{L'$', {KeyLeftShift, Key4}},
	{L'%', {KeyLeftShift, Key5}},
	{L'^', {KeyLeftShift, Key6}},
	{L'&', {KeyLeftShift, Key7}},
	{L'*', {KeyLeftShift, Key8}},
	{L'(', {KeyLeftShift, Key9}},
	{L')', {KeyLeftShift, Key0}},

	{L'£', {KeyLeftShift, KeyMinus}},			{L'-', {KeyMinus}},
	{L'+', {KeyLeftShift, KeyEquals}},			{L'=', {KeyEquals}},
	{L'|', {KeyLeftShift, KeyBackSlash}},		{L'\\', {KeyBackSlash}},

	{L'{', {KeyLeftShift, KeyOpenSquare}},		{L'[', {KeyOpenSquare}},
	{L'}', {KeyLeftShift, KeyCloseSquare}},		{L']', {KeyCloseSquare}},

	{L':', {KeyLeftShift, KeySemiColon}},		{L';', {KeySemiColon}},
	{L'"', {KeyLeftShift, KeyQuote}},			{L'\'', {KeyQuote}},

	{L'<', {KeyLeftShift, KeyComma}},			{L',', {KeyComma}},
	{L'>', {KeyLeftShift, KeyFullStop}},		{L'.', {KeyFullStop}},
	{L'?', {KeyLeftShift, KeyForwardSlash}},	{L'/', {KeyForwardSlash}},

	{L'0', {Key0}},	{L'1', {Key1}},	{L'2', {Key2}},	{L'3', {Key3}},	{L'4', {Key4}},
	{L'5', {Key5}},	{L'6', {Key6}},	{L'7', {Key7}},	{L'8', {Key8}},	{L'9', {Key9}},

	{L'A', {KeyLeftShift, KeyA}},	{L'B', {KeyLeftShift, KeyB}},	{L'C', {KeyLeftShift, KeyC}},
	{L'D', {KeyLeftShift, KeyD}},	{L'E', {KeyLeftShift, KeyE}},	{L'F', {KeyLeftShift, KeyF}},
	{L'G', {KeyLeftShift, KeyG}},	{L'H', {KeyLeftShift, KeyH}},	{L'I', {KeyLeftShift, KeyI}},
	{L'J', {KeyLeftShift, KeyJ}},	{L'K', {KeyLeftShift, KeyK}},	{L'L', {KeyLeftShift, KeyL}},
	{L'M', {KeyLeftShift, KeyM}},	{L'N', {KeyLeftShift, KeyN}},	{L'O', {KeyLeftShift, KeyO}},
	{L'P', {KeyLeftShift, KeyP}},	{L'Q', {KeyLeftShift, KeyQ}},	{L'R', {KeyLeftShift, KeyR}},
	{L'S', {KeyLeftShift, KeyS}},	{L'T', {KeyLeftShift, KeyT}},	{L'U', {KeyLeftShift, KeyU}},
	{L'V', {KeyLeftShift, KeyV}},	{L'W', {KeyLeftShift, KeyW}},	{L'X', {KeyLeftShift, KeyX}},
	{L'Y', {KeyLeftShift, KeyY}},	{L'Z', {KeyLeftShift, KeyZ}},

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
