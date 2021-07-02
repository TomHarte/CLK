//
//  Keyboard.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 17/06/2021.
//  Copyright © 2021 Thomas Harte. All rights reserved.
//

#include "Keyboard.hpp"

using namespace Enterprise;

uint16_t KeyboardMapper::mapped_key_for_key(Inputs::Keyboard::Key key) const {
#define BIND(source, dest)	case Inputs::Keyboard::Key::source:	return uint16_t(Key::dest)
	switch(key) {
		default: break;

		BIND(Backslash, Backslash);
		BIND(CapsLock, Lock);
		BIND(Tab, Tab);
		BIND(Escape, Escape);
		BIND(Hyphen, Hyphen);
		BIND(Equals, Caret);
		BIND(Backspace, Erase);
		BIND(Delete, Delete);
		BIND(Semicolon, Semicolon);
		BIND(Quote, Colon);
		BIND(OpenSquareBracket, OpenSquareBracket);
		BIND(CloseSquareBracket, CloseSquareBracket);

		BIND(End, Stop);
		BIND(Insert, Insert);
		BIND(BackTick, At);

		BIND(k1, k1);	BIND(k2, k2);	BIND(k3, k3);	BIND(k4, k4);	BIND(k5, k5);
		BIND(k6, k6);	BIND(k7, k7);	BIND(k8, k8);	BIND(k9, k9);	BIND(k0, k0);

		BIND(F1, F1);	BIND(F2, F2);	BIND(F3, F3);	BIND(F4, F4);
		BIND(F5, F5);	BIND(F6, F6);	BIND(F7, F7);	BIND(F8, F8);

		BIND(Keypad1, F1);	BIND(Keypad2, F2);	BIND(Keypad3, F3);	BIND(Keypad4, F4);
		BIND(Keypad5, F5);	BIND(Keypad6, F6);	BIND(Keypad7, F7);	BIND(Keypad8, F8);

		BIND(Q, Q);	BIND(W, W);	BIND(E, E);	BIND(R, R);	BIND(T, T);
		BIND(Y, Y);	BIND(U, U);	BIND(I, I);	BIND(O, O);	BIND(P, P);

		BIND(A, A);	BIND(S, S);	BIND(D, D);	BIND(F, F);	BIND(G, G);
		BIND(H, H);	BIND(J, J);	BIND(K, K);	BIND(L, L);

		BIND(Z, Z);	BIND(X, X);	BIND(C, C);	BIND(V, V);
		BIND(B, B);	BIND(N, N);	BIND(M, M);

		BIND(FullStop, FullStop);
		BIND(Comma, Comma);
		BIND(ForwardSlash, ForwardSlash);

		BIND(Space, Space);	BIND(Enter, Enter);

		BIND(LeftShift, LeftShift);
		BIND(RightShift, RightShift);
		BIND(LeftOption, Alt);
		BIND(RightOption, Alt);
		BIND(LeftControl, Control);
		BIND(RightControl, Control);

		BIND(Left, Left);
		BIND(Right, Right);
		BIND(Up, Up);
		BIND(Down, Down);
	}
#undef BIND

	return MachineTypes::MappedKeyboardMachine::KeyNotMapped;
}

const uint16_t *CharacterMapper::sequence_for_character(char character) const {
#define KEYS(x)		{uint16_t(x), MachineTypes::MappedKeyboardMachine::KeyEndSequence}
#define SHIFT(x)	{uint16_t(Key::LeftShift), uint16_t(x), MachineTypes::MappedKeyboardMachine::KeyEndSequence}
#define _			{MachineTypes::MappedKeyboardMachine::KeyNotMapped}
	static KeySequence key_sequences[] = {
		/* NUL */	_,							/* SOH */	_,
		/* STX */	_,							/* ETX */	_,
		/* EOT */	_,							/* ENQ */	_,
		/* ACK */	_,							/* BEL */	_,
		/* BS */	KEYS(Key::Erase),			/* HT */	KEYS(Key::Tab),
		/* LF */	KEYS(Key::Enter),			/* VT */	_,
		/* FF */	_,							/* CR */	KEYS(Key::Enter),
		/* SO */	_,							/* SI */	_,
		/* DLE */	_,							/* DC1 */	_,
		/* DC2 */	_,							/* DC3 */	_,
		/* DC4 */	_,							/* NAK */	_,
		/* SYN */	_,							/* ETB */	_,
		/* CAN */	_,							/* EM */	_,
		/* SUB */	_,							/* ESC */	KEYS(Key::Escape),
		/* FS */	_,							/* GS */	_,
		/* RS */	_,							/* US */	_,
		/* space */	KEYS(Key::Space),			/* ! */		SHIFT(Key::k1),
		/* " */		SHIFT(Key::k2),				/* # */		_,
		/* $ */		SHIFT(Key::k4),				/* % */		SHIFT(Key::k5),
		/* & */		SHIFT(Key::k6),				/* ' */		SHIFT(Key::k7),
		/* ( */		SHIFT(Key::k8),				/* ) */		SHIFT(Key::k9),
		/* * */		SHIFT(Key::Colon),			/* + */		SHIFT(Key::Semicolon),
		/* , */		KEYS(Key::Comma),			/* - */		KEYS(Key::Hyphen),
		/* . */		KEYS(Key::FullStop),		/* / */		KEYS(Key::ForwardSlash),
		/* 0 */		KEYS(Key::k0),				/* 1 */		KEYS(Key::k1),
		/* 2 */		KEYS(Key::k2),				/* 3 */		KEYS(Key::k3),
		/* 4 */		KEYS(Key::k4),				/* 5 */		KEYS(Key::k5),
		/* 6 */		KEYS(Key::k6),				/* 7 */		KEYS(Key::k7),
		/* 8 */		KEYS(Key::k8),				/* 9 */		KEYS(Key::k9),
		/* : */		KEYS(Key::Colon),			/* ; */		KEYS(Key::Semicolon),
		/* < */		SHIFT(Key::Comma),			/* = */		SHIFT(Key::Hyphen),
		/* > */		SHIFT(Key::FullStop),		/* ? */		SHIFT(Key::ForwardSlash),
		/* @ */		KEYS(Key::At),				/* A */		SHIFT(Key::A),
		/* B */		SHIFT(Key::B),				/* C */		SHIFT(Key::C),
		/* D */		SHIFT(Key::D),				/* E */		SHIFT(Key::E),
		/* F */		SHIFT(Key::F),				/* G */		SHIFT(Key::G),
		/* H */		SHIFT(Key::H),				/* I */		SHIFT(Key::I),
		/* J */		SHIFT(Key::J),				/* K */		SHIFT(Key::K),
		/* L */		SHIFT(Key::L),				/* M */		SHIFT(Key::M),
		/* N */		SHIFT(Key::N),				/* O */		SHIFT(Key::O),
		/* P */		SHIFT(Key::P),				/* Q */		SHIFT(Key::Q),
		/* R */		SHIFT(Key::R),				/* S */		SHIFT(Key::S),
		/* T */		SHIFT(Key::T),				/* U */		SHIFT(Key::U),
		/* V */		SHIFT(Key::V),				/* W */		SHIFT(Key::W),
		/* X */		SHIFT(Key::X),				/* Y */		SHIFT(Key::Y),
		/* Z */		SHIFT(Key::Z),				/* [ */		KEYS(Key::OpenSquareBracket),
		/* \ */		KEYS(Key::Backslash),		/* ] */		KEYS(Key::CloseSquareBracket),
		/* ^ */		SHIFT(Key::Caret),			/* _ */		SHIFT(Key::k0),
		/* ` */		SHIFT(Key::At),				/* a */		KEYS(Key::A),
		/* b */		KEYS(Key::B),				/* c */		KEYS(Key::C),
		/* d */		KEYS(Key::D),				/* e */		KEYS(Key::E),
		/* f */		KEYS(Key::F),				/* g */		KEYS(Key::G),
		/* h */		KEYS(Key::H),				/* i */		KEYS(Key::I),
		/* j */		KEYS(Key::J),				/* k */		KEYS(Key::K),
		/* l */		KEYS(Key::L),				/* m */		KEYS(Key::M),
		/* n */		KEYS(Key::N),				/* o */		KEYS(Key::O),
		/* p */		KEYS(Key::P),				/* q */		KEYS(Key::Q),
		/* r */		KEYS(Key::R),				/* s */		KEYS(Key::S),
		/* t */		KEYS(Key::T),				/* u */		KEYS(Key::U),
		/* v */		KEYS(Key::V),				/* w */		KEYS(Key::W),
		/* x */		KEYS(Key::X),				/* y */		KEYS(Key::Y),
		/* z */		KEYS(Key::Z),				/* { */		SHIFT(Key::OpenSquareBracket),
		/* | */		SHIFT(Key::Backslash),		/* } */		SHIFT(Key::CloseSquareBracket),
		/* ~ */		SHIFT(Key::Caret)
	};
#undef _
#undef SHIFT
#undef KEYS

	return table_lookup_sequence_for_character(key_sequences, character);
}
