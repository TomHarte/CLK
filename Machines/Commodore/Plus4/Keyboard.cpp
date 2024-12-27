//
//  Keyboard.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 13/12/2024.
//  Copyright 2024 Thomas Harte. All rights reserved.
//

#include "Keyboard.hpp"

using namespace Commodore::Plus4;

uint16_t KeyboardMapper::mapped_key_for_key(Inputs::Keyboard::Key key) const {
#define BIND(source, dest)	case Inputs::Keyboard::Key::source:	return Commodore::Plus4::dest
	switch(key) {
		default: break;

		BIND(k0, k0);		BIND(k1, k1);		BIND(k2, k2);		BIND(k3, k3);		BIND(k4, k4);
		BIND(k5, k5);		BIND(k6, k6);		BIND(k7, k7);		BIND(k8, k8);		BIND(k9, k9);
		BIND(Q, Q);			BIND(W, W);			BIND(E, E);			BIND(R, R);			BIND(T, T);
		BIND(Y, Y);			BIND(U, U);			BIND(I, I);			BIND(O, O);			BIND(P, P);
		BIND(A, A);			BIND(S, S);			BIND(D, D);			BIND(F, F);			BIND(G, G);
		BIND(H, H);			BIND(J, J);			BIND(K, K);			BIND(L, L);
		BIND(Z, Z);			BIND(X, X);			BIND(C, C);			BIND(V, V);
		BIND(B, B);			BIND(N, N);			BIND(M, M);

		BIND(Backspace, InsDel);
		BIND(Escape, Escape);
		BIND(F1, F1_F4);
		BIND(F2, F2_F5);
		BIND(F3, F3_F6);
		BIND(F4, Help_F7);
		BIND(Enter, Return);
		BIND(Space, Space);

		BIND(Up, Up);		BIND(Down, Down);	BIND(Left, Left);	BIND(Right, Right);

		BIND(LeftShift, Shift);			BIND(RightShift, Shift);
		BIND(LeftControl, Control);		BIND(RightControl, Control);
		BIND(LeftOption, Commodore);	BIND(RightOption, Commodore);

		BIND(FullStop, FullStop);		BIND(Comma, Comma);
		BIND(Semicolon, Semicolon);		BIND(Quote, Colon);
		BIND(Equals, Equals);			BIND(ForwardSlash, Slash);

		BIND(OpenSquareBracket, At);
		BIND(CloseSquareBracket, Plus);
		BIND(Backslash, Clear_Home);

		BIND(F11, Clear_Home);
		BIND(F12, Run_Stop);

		// TODO:
		//	GBP
		//	Asterisk
	}
#undef BIND
	return MachineTypes::MappedKeyboardMachine::KeyNotMapped;
}

//const uint16_t *CharacterMapper::sequence_for_character(char character) const {
//#define KEYS(...)	{__VA_ARGS__, MachineTypes::MappedKeyboardMachine::KeyEndSequence}
//#define SHIFT(...)	{KeyLShift, __VA_ARGS__, MachineTypes::MappedKeyboardMachine::KeyEndSequence}
//#define X			{MachineTypes::MappedKeyboardMachine::KeyNotMapped}
//	static KeySequence key_sequences[] = {
//		/* NUL */	X,							/* SOH */	X,
//		/* STX */	X,							/* ETX */	X,
//		/* EOT */	X,							/* ENQ */	X,
//		/* ACK */	X,							/* BEL */	X,
//		/* BS */	KEYS(KeyDelete),			/* HT */	X,
//		/* LF */	KEYS(KeyReturn),			/* VT */	X,
//		/* FF */	X,							/* CR */	X,
//		/* SO */	X,							/* SI */	X,
//		/* DLE */	X,							/* DC1 */	X,
//		/* DC2 */	X,							/* DC3 */	X,
//		/* DC4 */	X,							/* NAK */	X,
//		/* SYN */	X,							/* ETB */	X,
//		/* CAN */	X,							/* EM */	X,
//		/* SUB */	X,							/* ESC */	X,
//		/* FS */	X,							/* GS */	X,
//		/* RS */	X,							/* US */	X,
//		/* space */	KEYS(KeySpace),				/* ! */		SHIFT(Key1),
//		/* " */		SHIFT(Key2),				/* # */		SHIFT(Key3),
//		/* $ */		SHIFT(Key4),				/* % */		SHIFT(Key5),
//		/* & */		SHIFT(Key6),				/* ' */		SHIFT(Key7),
//		/* ( */		SHIFT(Key8),				/* ) */		SHIFT(Key9),
//		/* * */		KEYS(KeyAsterisk),			/* + */		KEYS(KeyPlus),
//		/* , */		KEYS(KeyComma),				/* - */		KEYS(KeyDash),
//		/* . */		KEYS(KeyFullStop),			/* / */		KEYS(KeySlash),
//		/* 0 */		KEYS(Key0),					/* 1 */		KEYS(Key1),
//		/* 2 */		KEYS(Key2),					/* 3 */		KEYS(Key3),
//		/* 4 */		KEYS(Key4),					/* 5 */		KEYS(Key5),
//		/* 6 */		KEYS(Key6),					/* 7 */		KEYS(Key7),
//		/* 8 */		KEYS(Key8),					/* 9 */		KEYS(Key9),
//		/* : */		KEYS(KeyColon),				/* ; */		KEYS(KeySemicolon),
//		/* < */		SHIFT(KeyComma),			/* = */		KEYS(KeyEquals),
//		/* > */		SHIFT(KeyFullStop),			/* ? */		SHIFT(KeySlash),
//		/* @ */		KEYS(KeyAt),				/* A */		KEYS(KeyA),
//		/* B */		KEYS(KeyB),					/* C */		KEYS(KeyC),
//		/* D */		KEYS(KeyD),					/* E */		KEYS(KeyE),
//		/* F */		KEYS(KeyF),					/* G */		KEYS(KeyG),
//		/* H */		KEYS(KeyH),					/* I */		KEYS(KeyI),
//		/* J */		KEYS(KeyJ),					/* K */		KEYS(KeyK),
//		/* L */		KEYS(KeyL),					/* M */		KEYS(KeyM),
//		/* N */		KEYS(KeyN),					/* O */		KEYS(KeyO),
//		/* P */		KEYS(KeyP),					/* Q */		KEYS(KeyQ),
//		/* R */		KEYS(KeyR),					/* S */		KEYS(KeyS),
//		/* T */		KEYS(KeyT),					/* U */		KEYS(KeyU),
//		/* V */		KEYS(KeyV),					/* W */		KEYS(KeyW),
//		/* X */		KEYS(KeyX),					/* Y */		KEYS(KeyY),
//		/* Z */		KEYS(KeyZ),					/* [ */		SHIFT(KeyColon),
//		/* \ */		X,							/* ] */		SHIFT(KeySemicolon),
//		/* ^ */		X,							/* _ */		X,
//		/* ` */		X,							/* a */		KEYS(KeyA),
//		/* b */		KEYS(KeyB),					/* c */		KEYS(KeyC),
//		/* d */		KEYS(KeyD),					/* e */		KEYS(KeyE),
//		/* f */		KEYS(KeyF),					/* g */		KEYS(KeyG),
//		/* h */		KEYS(KeyH),					/* i */		KEYS(KeyI),
//		/* j */		KEYS(KeyJ),					/* k */		KEYS(KeyK),
//		/* l */		KEYS(KeyL),					/* m */		KEYS(KeyM),
//		/* n */		KEYS(KeyN),					/* o */		KEYS(KeyO),
//		/* p */		KEYS(KeyP),					/* q */		KEYS(KeyQ),
//		/* r */		KEYS(KeyR),					/* s */		KEYS(KeyS),
//		/* t */		KEYS(KeyT),					/* u */		KEYS(KeyU),
//		/* v */		KEYS(KeyV),					/* w */		KEYS(KeyW),
//		/* x */		KEYS(KeyX),					/* y */		KEYS(KeyY),
//		/* z */		KEYS(KeyZ)
//	};
//#undef KEYS
//#undef SHIFT
//#undef X
//
//	return table_lookup_sequence_for_character(key_sequences, character);
//}
