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
		BIND(BackTick, Asterisk);

		BIND(F11, Clear_Home);
		BIND(F12, Run_Stop);

		// TODO:
		//	GBP
	}
#undef BIND
	return MachineTypes::MappedKeyboardMachine::KeyNotMapped;
}

const uint16_t *CharacterMapper::sequence_for_character(char character) const {
	static constexpr KeySequence X = { MachineTypes::MappedKeyboardMachine::KeyNotMapped };
	const auto key = [](Key k) -> KeySequence {
		return { k, MachineTypes::MappedKeyboardMachine::KeyEndSequence };
	};
	const auto shift = [](Key k) -> KeySequence {
		return { Key::Shift, k, MachineTypes::MappedKeyboardMachine::KeyEndSequence };
	};

	static KeySequence key_sequences[] = {
		/* NUL */	X,							/* SOH */	X,
		/* STX */	X,							/* ETX */	X,
		/* EOT */	X,							/* ENQ */	X,
		/* ACK */	X,							/* BEL */	X,
		/* BS */	key(Key::InsDel),			/* HT */	X,
		/* LF */	key(Key::Return),			/* VT */	X,
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
		/* space */	key(Key::Space),			/* ! */		shift(Key::k1),
		/* " */		shift(Key::k2),				/* # */		shift(Key::k3),
		/* $ */		shift(Key::k4),				/* % */		shift(Key::k5),
		/* & */		shift(Key::k6),				/* ' */		shift(Key::k7),
		/* ( */		shift(Key::k8),				/* ) */		shift(Key::k9),
		/* * */		key(Key::Asterisk),		/* + */		key(Key::Plus),
		/* , */		key(Key::Comma),			/* - */		key(Key::Minus),
		/* . */		key(Key::FullStop),		/* / */		key(Key::Slash),
		/* 0 */		key(Key::k0),				/* 1 */		key(Key::k1),
		/* 2 */		key(Key::k2),				/* 3 */		key(Key::k3),
		/* 4 */		key(Key::k4),				/* 5 */		key(Key::k5),
		/* 6 */		key(Key::k6),				/* 7 */		key(Key::k7),
		/* 8 */		key(Key::k8),				/* 9 */		key(Key::k9),
		/* : */		key(Key::Colon),			/* ; */		key(Key::Semicolon),
		/* < */		shift(Key::Comma),			/* = */		key(Key::Equals),
		/* > */		shift(Key::FullStop),		/* ? */		shift(Key::Slash),
		/* @ */		key(Key::At),				/* A */		key(Key::A),
		/* B */		key(Key::B),				/* C */		key(Key::C),
		/* D */		key(Key::D),				/* E */		key(Key::E),
		/* F */		key(Key::F),				/* G */		key(Key::G),
		/* H */		key(Key::H),				/* I */		key(Key::I),
		/* J */		key(Key::J),				/* K */		key(Key::K),
		/* L */		key(Key::L),				/* M */		key(Key::M),
		/* N */		key(Key::N),				/* O */		key(Key::O),
		/* P */		key(Key::P),				/* Q */		key(Key::Q),
		/* R */		key(Key::R),				/* S */		key(Key::S),
		/* T */		key(Key::T),				/* U */		key(Key::U),
		/* V */		key(Key::V),				/* W */		key(Key::W),
		/* X */		key(Key::X),				/* Y */		key(Key::Y),
		/* Z */		key(Key::Z),				/* [ */		shift(Key::Colon),
		/* \ */		X,							/* ] */		shift(Key::Semicolon),
		/* ^ */		X,							/* _ */		X,
		/* ` */		X,							/* a */		key(Key::A),
		/* b */		key(Key::B),				/* c */		key(Key::C),
		/* d */		key(Key::D),				/* e */		key(Key::E),
		/* f */		key(Key::F),				/* g */		key(Key::G),
		/* h */		key(Key::H),				/* i */		key(Key::I),
		/* j */		key(Key::J),				/* k */		key(Key::K),
		/* l */		key(Key::L),				/* m */		key(Key::M),
		/* n */		key(Key::N),				/* o */		key(Key::O),
		/* p */		key(Key::P),				/* q */		key(Key::Q),
		/* r */		key(Key::R),				/* s */		key(Key::S),
		/* t */		key(Key::T),				/* u */		key(Key::U),
		/* v */		key(Key::V),				/* w */		key(Key::W),
		/* x */		key(Key::X),				/* y */		key(Key::Y),
		/* z */		key(Key::Z)
	};

	return table_lookup_sequence_for_character(key_sequences, character);
}
