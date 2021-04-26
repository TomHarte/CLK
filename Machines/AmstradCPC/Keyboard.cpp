//
//  Keyboard.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 10/10/2017.
//  Copyright 2017 Thomas Harte. All rights reserved.
//

#include "Keyboard.hpp"

using namespace AmstradCPC;

uint16_t KeyboardMapper::mapped_key_for_key(Inputs::Keyboard::Key key) const {
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

const uint16_t *CharacterMapper::sequence_for_character(char character) const {
#define KEYS(...)	{__VA_ARGS__, MachineTypes::MappedKeyboardMachine::KeyEndSequence}
#define SHIFT(...)	{KeyShift, __VA_ARGS__, MachineTypes::MappedKeyboardMachine::KeyEndSequence}
#define X			{MachineTypes::MappedKeyboardMachine::KeyNotMapped}
	static KeySequence key_sequences[] = {
		/* NUL */	X,							/* SOH */	X,
		/* STX */	X,							/* ETX */	X,
		/* EOT */	X,							/* ENQ */	X,
		/* ACK */	X,							/* BEL */	X,
		/* BS */	KEYS(KeyDelete),			/* HT */	X,
		/* LF */	KEYS(KeyReturn),			/* VT */	X,
		/* FF */	X,							/* CR */	KEYS(KeyReturn),
		/* SO */	X,							/* SI */	X,
		/* DLE */	X,							/* DC1 */	X,
		/* DC2 */	X,							/* DC3 */	X,
		/* DC4 */	X,							/* NAK */	X,
		/* SYN */	X,							/* ETB */	X,
		/* CAN */	X,							/* EM */	X,
		/* SUB */	X,							/* ESC */	X,
		/* FS */	X,							/* GS */	X,
		/* RS */	X,							/* US */	X,
		/* space */	KEYS(KeySpace),				/* ! */		SHIFT(Key1),
		/* " */		SHIFT(Key2),				/* # */		SHIFT(Key3),
		/* $ */		SHIFT(Key4),				/* % */		SHIFT(Key5),
		/* & */		SHIFT(Key6),				/* ' */		SHIFT(Key7),
		/* ( */		SHIFT(Key8),				/* ) */		SHIFT(Key9),
		/* * */		SHIFT(KeyColon),			/* + */		SHIFT(KeySemicolon),
		/* , */		KEYS(KeyComma),				/* - */		KEYS(KeyMinus),
		/* . */		KEYS(KeyFullStop),			/* / */		KEYS(KeyForwardSlash),
		/* 0 */		KEYS(Key0),					/* 1 */		KEYS(Key1),
		/* 2 */		KEYS(Key2),					/* 3 */		KEYS(Key3),
		/* 4 */		KEYS(Key4),					/* 5 */		KEYS(Key5),
		/* 6 */		KEYS(Key6),					/* 7 */		KEYS(Key7),
		/* 8 */		KEYS(Key8),					/* 9 */		KEYS(Key9),
		/* : */		KEYS(KeyColon),				/* ; */		KEYS(KeySemicolon),
		/* < */		SHIFT(KeyComma),			/* = */		SHIFT(KeyMinus),
		/* > */		SHIFT(KeyFullStop),			/* ? */		SHIFT(KeyForwardSlash),
		/* @ */		SHIFT(KeyAt),				/* A */		SHIFT(KeyA),
		/* B */		SHIFT(KeyB),				/* C */		SHIFT(KeyC),
		/* D */		SHIFT(KeyD),				/* E */		SHIFT(KeyE),
		/* F */		SHIFT(KeyF),				/* G */		SHIFT(KeyG),
		/* H */		SHIFT(KeyH),				/* I */		SHIFT(KeyI),
		/* J */		SHIFT(KeyJ),				/* K */		SHIFT(KeyK),
		/* L */		SHIFT(KeyL),				/* M */		SHIFT(KeyM),
		/* N */		SHIFT(KeyN),				/* O */		SHIFT(KeyO),
		/* P */		SHIFT(KeyP),				/* Q */		SHIFT(KeyQ),
		/* R */		SHIFT(KeyR),				/* S */		SHIFT(KeyS),
		/* T */		SHIFT(KeyT),				/* U */		SHIFT(KeyU),
		/* V */		SHIFT(KeyV),				/* W */		SHIFT(KeyW),
		/* X */		SHIFT(KeyX),				/* Y */		SHIFT(KeyY),
		/* Z */		SHIFT(KeyZ),				/* [ */		KEYS(KeyLeftSquareBracket),
		/* \ */		KEYS(KeyBackSlash),			/* ] */		KEYS(KeyRightSquareBracket),
		/* ^ */		SHIFT(KeyCaret),			/* _ */		SHIFT(Key0),
		/* ` */		X,							/* a */		KEYS(KeyA),
		/* b */		KEYS(KeyB),					/* c */		KEYS(KeyC),
		/* d */		KEYS(KeyD),					/* e */		KEYS(KeyE),
		/* f */		KEYS(KeyF),					/* g */		KEYS(KeyG),
		/* h */		KEYS(KeyH),					/* i */		KEYS(KeyI),
		/* j */		KEYS(KeyJ),					/* k */		KEYS(KeyK),
		/* l */		KEYS(KeyL),					/* m */		KEYS(KeyM),
		/* n */		KEYS(KeyN),					/* o */		KEYS(KeyO),
		/* p */		KEYS(KeyP),					/* q */		KEYS(KeyQ),
		/* r */		KEYS(KeyR),					/* s */		KEYS(KeyS),
		/* t */		KEYS(KeyT),					/* u */		KEYS(KeyU),
		/* v */		KEYS(KeyV),					/* w */		KEYS(KeyW),
		/* x */		KEYS(KeyX),					/* y */		KEYS(KeyY),
		/* z */		KEYS(KeyZ),					/* { */		SHIFT(KeyLeftSquareBracket),
		/* | */		SHIFT(KeyAt),				/* } */		SHIFT(KeyRightSquareBracket),
		/* ~ */		X
	};
#undef KEYS
#undef SHIFT
#undef X

	return table_lookup_sequence_for_character(key_sequences, character);
}

bool CharacterMapper::needs_pause_after_key(uint16_t key) const {
	return key != KeyControl && key != KeyShift;
}
