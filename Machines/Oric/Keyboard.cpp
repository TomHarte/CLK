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

const uint16_t *CharacterMapper::sequence_for_character(char character) const {
#define KEYS(...)	{__VA_ARGS__, MachineTypes::MappedKeyboardMachine::KeyEndSequence}
#define SHIFT(...)	{KeyLeftShift, __VA_ARGS__, MachineTypes::MappedKeyboardMachine::KeyEndSequence}
#define X			{MachineTypes::MappedKeyboardMachine::KeyNotMapped}
	static KeySequence key_sequences[] = {
		/* NUL */	X,							/* SOH */	X,
		/* STX */	X,							/* ETX */	X,
		/* EOT */	X,							/* ENQ */	X,
		/* ACK */	X,							/* BEL */	X,
		/* BS */	KEYS(KeyDelete),			/* HT */	X,
		/* LF */	KEYS(KeyReturn),			/* VT */	X,
		/* FF */	X,							/* CR */	X,
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
		/* " */		SHIFT(KeyQuote),			/* # */		SHIFT(Key3),
		/* $ */		SHIFT(Key4),				/* % */		SHIFT(Key5),
		/* & */		SHIFT(Key7),				/* ' */		KEYS(KeyQuote),
		/* ( */		SHIFT(Key9),				/* ) */		SHIFT(Key0),
		/* * */		SHIFT(Key8),				/* + */		SHIFT(KeyEquals),
		/* , */		KEYS(KeyComma),				/* - */		KEYS(KeyMinus),
		/* . */		KEYS(KeyFullStop),			/* / */		KEYS(KeyForwardSlash),
		/* 0 */		KEYS(Key0),					/* 1 */		KEYS(Key1),
		/* 2 */		KEYS(Key2),					/* 3 */		KEYS(Key3),
		/* 4 */		KEYS(Key4),					/* 5 */		KEYS(Key5),
		/* 6 */		KEYS(Key6),					/* 7 */		KEYS(Key7),
		/* 8 */		KEYS(Key8),					/* 9 */		KEYS(Key9),
		/* : */		SHIFT(KeySemiColon),		/* ; */		KEYS(KeySemiColon),
		/* < */		SHIFT(KeyComma),			/* = */		KEYS(KeyEquals),
		/* > */		SHIFT(KeyFullStop),			/* ? */		SHIFT(KeyForwardSlash),
		/* @ */		SHIFT(Key2),				/* A */		SHIFT(KeyA),
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
		/* Z */		SHIFT(KeyZ),				/* [ */		KEYS(KeyOpenSquare),
		/* \ */		KEYS(KeyBackSlash),			/* ] */		KEYS(KeyCloseSquare),
		/* ^ */		SHIFT(Key6),				/* _ */		X,
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
		/* z */		KEYS(KeyZ),					/* { */		SHIFT(KeyOpenSquare),
		/* | */		SHIFT(KeyBackSlash),		/* } */		SHIFT(KeyCloseSquare),
	};
#undef KEYS
#undef SHIFT
#undef X

	return table_lookup_sequence_for_character(key_sequences, sizeof(key_sequences), character);
}
