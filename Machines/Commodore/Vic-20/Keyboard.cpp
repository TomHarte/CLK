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

const uint16_t *CharacterMapper::sequence_for_character(char character) const {
#define KEYS(...)	{__VA_ARGS__, MachineTypes::MappedKeyboardMachine::KeyEndSequence}
#define SHIFT(...)	{KeyLShift, __VA_ARGS__, MachineTypes::MappedKeyboardMachine::KeyEndSequence}
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
		/* " */		SHIFT(Key2),				/* # */		SHIFT(Key3),
		/* $ */		SHIFT(Key4),				/* % */		SHIFT(Key5),
		/* & */		SHIFT(Key6),				/* ' */		SHIFT(Key7),
		/* ( */		SHIFT(Key8),				/* ) */		SHIFT(Key9),
		/* * */		KEYS(KeyAsterisk),			/* + */		KEYS(KeyPlus),
		/* , */		KEYS(KeyComma),				/* - */		KEYS(KeyDash),
		/* . */		KEYS(KeyFullStop),			/* / */		KEYS(KeySlash),
		/* 0 */		KEYS(Key0),					/* 1 */		KEYS(Key1),
		/* 2 */		KEYS(Key2),					/* 3 */		KEYS(Key3),
		/* 4 */		KEYS(Key4),					/* 5 */		KEYS(Key5),
		/* 6 */		KEYS(Key6),					/* 7 */		KEYS(Key7),
		/* 8 */		KEYS(Key8),					/* 9 */		KEYS(Key9),
		/* : */		KEYS(KeyColon),				/* ; */		KEYS(KeySemicolon),
		/* < */		SHIFT(KeyComma),			/* = */		KEYS(KeyEquals),
		/* > */		SHIFT(KeyFullStop),			/* ? */		SHIFT(KeySlash),
		/* @ */		KEYS(KeyAt),				/* A */		KEYS(KeyA),
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
		/* Z */		KEYS(KeyZ),					/* [ */		SHIFT(KeyColon),
		/* \ */		X,							/* ] */		SHIFT(KeySemicolon),
		/* ^ */		X,							/* _ */		X,
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
		/* z */		KEYS(KeyZ)
	};
#undef KEYS
#undef SHIFT
#undef X

	return table_lookup_sequence_for_character(key_sequences, sizeof(key_sequences), character);
}
