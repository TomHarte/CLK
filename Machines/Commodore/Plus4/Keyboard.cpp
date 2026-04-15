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
#define BIND(source, dest)	case Inputs::Keyboard::Key::source:	return Commodore::Plus4::Key::dest
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

namespace {
const std::unordered_map<wchar_t, const std::vector<uint16_t>> sequences = {
	{L'\b', {Key::InsDel}},
	{L'\n', {Key::Return}},	{L'\r', {Key::Return}},
	{L' ', {Key::Space}},

	{L'!', {Key::Shift, Key::k1}},
	{L'"', {Key::Shift, Key::k2}},
	{L'#', {Key::Shift, Key::k3}},
	{L'$', {Key::Shift, Key::k4}},
	{L'%', {Key::Shift, Key::k5}},
	{L'&', {Key::Shift, Key::k6}},
	{L'\'', {Key::Shift, Key::k7}},
	{L'(', {Key::Shift, Key::k8}},
	{L')', {Key::Shift, Key::k9}},
	{L'↑', {Key::Shift, Key::k0}},

	{L'@', {Key::At}},
	{L'+', {Key::Plus}},
	{L'-', {Key::Minus}},

	{L'[', {Key::Shift, Key::Colon}},		{L':', {Key::Colon}},
	{L']', {Key::Shift, Key::Semicolon}},	{L';', {Key::Semicolon}},
	{L'*', {Key::Asterisk}},

	{L'<', {Key::Shift, Key::Comma}},		{L',', {Key::Comma}},
	{L'>', {Key::Shift, Key::FullStop}},	{L'.', {Key::FullStop}},
	{L'?', {Key::Shift, Key::Slash}},		{L'/', {Key::Slash}},

	{L'£', {Key::GBP}},
	{L'=', {Key::Equals}},

	{L'0', {Key::k0}},	{L'1', {Key::k1}},	{L'2', {Key::k2}},	{L'3', {Key::k3}},	{L'4', {Key::k4}},
	{L'5', {Key::k5}},	{L'6', {Key::k6}},	{L'7', {Key::k7}},	{L'8', {Key::k8}},	{L'9', {Key::k9}},

	{L'a', {Key::A}},	{L'b', {Key::B}},	{L'c', {Key::C}},
	{L'd', {Key::D}},	{L'e', {Key::E}},	{L'f', {Key::F}},
	{L'g', {Key::G}},	{L'h', {Key::H}},	{L'i', {Key::I}},
	{L'j', {Key::J}},	{L'k', {Key::K}},	{L'l', {Key::L}},
	{L'm', {Key::M}},	{L'n', {Key::N}},	{L'o', {Key::O}},
	{L'p', {Key::P}},	{L'q', {Key::Q}},	{L'r', {Key::R}},
	{L's', {Key::S}},	{L't', {Key::T}},	{L'u', {Key::U}},
	{L'v', {Key::V}},	{L'w', {Key::W}},	{L'x', {Key::X}},
	{L'y', {Key::Y}},	{L'z', {Key::Z}},
};
}

std::span<const uint16_t> CharacterMapper::sequence_for_character(const wchar_t character) const {
	return lookup_sequence(sequences, character);
}
