//
//  Keyboard.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 10/10/2017.
//  Copyright 2017 Thomas Harte. All rights reserved.
//

#include "Keyboard.hpp"

using namespace Electron;

uint16_t KeyboardMapper::mapped_key_for_key(Inputs::Keyboard::Key key) {
#define BIND(source, dest)	case Inputs::Keyboard::Key::source:	return Electron::Key::dest
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
	return KeyboardMachine::MappedMachine::KeyNotMapped;
}

uint16_t *CharacterMapper::sequence_for_character(char character) {
#define KEYS(...)	{__VA_ARGS__, KeyboardMachine::MappedMachine::KeyEndSequence}
#define SHIFT(...)	{KeyShift, __VA_ARGS__, KeyboardMachine::MappedMachine::KeyEndSequence}
#define CTRL(...)	{KeyControl, __VA_ARGS__, KeyboardMachine::MappedMachine::KeyEndSequence}
#define X			{KeyboardMachine::MappedMachine::KeyNotMapped}
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
		/* * */		SHIFT(KeyColon),			/* + */		SHIFT(KeySemiColon),
		/* , */		KEYS(KeyComma),				/* - */		KEYS(KeyMinus),
		/* . */		KEYS(KeyFullStop),			/* / */		KEYS(KeySlash),
		/* 0 */		KEYS(Key0),					/* 1 */		KEYS(Key1),
		/* 2 */		KEYS(Key2),					/* 3 */		KEYS(Key3),
		/* 4 */		KEYS(Key4),					/* 5 */		KEYS(Key5),
		/* 6 */		KEYS(Key6),					/* 7 */		KEYS(Key7),
		/* 8 */		KEYS(Key8),					/* 9 */		KEYS(Key9),
		/* : */		KEYS(KeyColon),				/* ; */		KEYS(KeySemiColon),
		/* < */		SHIFT(KeyComma),			/* = */		SHIFT(KeyMinus),
		/* > */		SHIFT(KeyFullStop),			/* ? */		SHIFT(KeySlash),
		/* @ */		SHIFT(Key0),				/* A */		KEYS(KeyA),
		/* B */		KEYS(KeyB),					/* C */		KEYS(KeyC),
		/* D */		KEYS(KeyD),					/* E */		KEYS(KeyE),
		/* F */		KEYS(KeyF),					/* G */		KEYS(KeyG),
		/* H */		KEYS(KeyH),					/* I */		KEYS(KeyI),
		/* J */		KEYS(KeyJ),					/* K */		KEYS(KeyK),
		/* L */		KEYS(KeyL),					/* M */		KEYS(KeyM),
		/* N */		KEYS(KeyN),					/* O */		KEYS(KeyO),
		/* P */		KEYS(KeyP),					/* Q */		KEYS(KeyQ),
		/* R */		KEYS(KeyR),					/* S */		KEYS(KeyS),
		/* T */		KEYS(KeyT),					/* U */		KEYS(KeyU),
		/* V */		KEYS(KeyV),					/* W */		KEYS(KeyW),
		/* X */		KEYS(KeyX),					/* Y */		KEYS(KeyY),
		/* Z */		KEYS(KeyZ),					/* [ */		SHIFT(KeyCopy),
		/* \ */		CTRL(KeyRight),				/* ] */		CTRL(KeyCopy),
		/* ^ */		SHIFT(KeyLeft),				/* _ */		SHIFT(KeyDown),
		/* ` */		X,							/* a */		SHIFT(KeyA),
		/* b */		SHIFT(KeyB),				/* c */		SHIFT(KeyC),
		/* d */		SHIFT(KeyD),				/* e */		SHIFT(KeyE),
		/* f */		SHIFT(KeyF),				/* g */		SHIFT(KeyG),
		/* h */		SHIFT(KeyH),				/* i */		SHIFT(KeyI),
		/* j */		SHIFT(KeyJ),				/* k */		SHIFT(KeyK),
		/* l */		SHIFT(KeyL),				/* m */		SHIFT(KeyM),
		/* n */		SHIFT(KeyN),				/* o */		SHIFT(KeyO),
		/* p */		SHIFT(KeyP),				/* q */		SHIFT(KeyQ),
		/* r */		SHIFT(KeyR),				/* s */		SHIFT(KeyS),
		/* t */		SHIFT(KeyT),				/* u */		SHIFT(KeyU),
		/* v */		SHIFT(KeyV),				/* w */		SHIFT(KeyW),
		/* x */		SHIFT(KeyX),				/* y */		SHIFT(KeyY),
		/* z */		SHIFT(KeyZ),				/* { */		CTRL(KeyUp),
		/* | */		SHIFT(KeyRight),			/* } */		CTRL(KeyDown),
		/* ~ */		CTRL(KeyLeft)
	};
#undef KEYS
#undef SHIFT
#undef X

	return table_lookup_sequence_for_character(key_sequences, sizeof(key_sequences), character);
}

bool CharacterMapper::needs_pause_after_key(uint16_t key) {
	return key != KeyControl && key != KeyShift && key != KeyFunc;
}
