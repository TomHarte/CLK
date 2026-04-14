//
//  Keyboard.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 17/06/2021.
//  Copyright © 2021 Thomas Harte. All rights reserved.
//

#include "Keyboard.hpp"

using namespace Enterprise;

uint16_t KeyboardMapper::mapped_key_for_key(const Inputs::Keyboard::Key key) const {
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

namespace {
const std::unordered_map<wchar_t, const std::vector<uint16_t>> sequences = {
	{L'\b', {Key::Erase}},	{L'\t', {Key::Tab}},
	{L'\n', {Key::Enter}},	{L'\r', {Key::Enter}},
	{L'\e', {Key::Escape}},
	{L' ', {Key::Space}},

	{L'!', {Key::LeftShift, Key::k1}},
	{L'"', {Key::LeftShift, Key::k2}},
	{L'£', {Key::LeftShift, Key::k3}},
	{L'$', {Key::LeftShift, Key::k4}},
	{L'%', {Key::LeftShift, Key::k5}},
	{L'&', {Key::LeftShift, Key::k6}},
	{L'\'', {Key::LeftShift, Key::k7}},
	{L'(', {Key::LeftShift, Key::k8}},
	{L')', {Key::LeftShift, Key::k9}},
	{L'_', {Key::LeftShift, Key::k0}},

	{L'=', {Key::LeftShift, Key::Hyphen}},
	{L'-', {Key::Hyphen}},
	{L'~', {Key::LeftShift, Key::Caret}},
	{L'^', {Key::Caret}},

	{L'`', {Key::LeftShift, Key::At}},
	{L'@', {Key::At}},
	{L'{', {Key::LeftShift, Key::OpenSquareBracket}},
	{L'[', {Key::OpenSquareBracket}},

	{L'+', {Key::LeftShift, Key::Semicolon}},
	{L';', {Key::Semicolon}},
	{L'*', {Key::LeftShift, Key::Colon}},
	{L':', {Key::Colon}},
	{L'}', {Key::LeftShift, Key::CloseSquareBracket}},
	{L']', {Key::CloseSquareBracket}},

	{L'|', {Key::LeftShift, Key::Backslash}},
	{L'\\', {Key::Backslash}},
	{L'<', {Key::LeftShift, Key::Comma}},
	{L',', {Key::Comma}},
	{L'>', {Key::LeftShift, Key::FullStop}},
	{L'.', {Key::FullStop}},
	{L'?', {Key::LeftShift, Key::ForwardSlash}},
	{L'/', {Key::ForwardSlash}},

	{L'0', {Key::k0}},	{L'1', {Key::k1}},	{L'2', {Key::k2}},	{L'3', {Key::k3}},	{L'4', {Key::k4}},
	{L'5', {Key::k5}},	{L'6', {Key::k6}},	{L'7', {Key::k7}},	{L'8', {Key::k8}},	{L'9', {Key::k9}},

	{L'A', {Key::LeftShift, Key::A}},	{L'B', {Key::LeftShift, Key::B}},	{L'C', {Key::LeftShift, Key::C}},
	{L'D', {Key::LeftShift, Key::D}},	{L'E', {Key::LeftShift, Key::E}},	{L'F', {Key::LeftShift, Key::F}},
	{L'G', {Key::LeftShift, Key::G}},	{L'H', {Key::LeftShift, Key::H}},	{L'I', {Key::LeftShift, Key::I}},
	{L'J', {Key::LeftShift, Key::J}},	{L'K', {Key::LeftShift, Key::K}},	{L'L', {Key::LeftShift, Key::L}},
	{L'M', {Key::LeftShift, Key::M}},	{L'N', {Key::LeftShift, Key::N}},	{L'O', {Key::LeftShift, Key::O}},
	{L'P', {Key::LeftShift, Key::P}},	{L'Q', {Key::LeftShift, Key::Q}},	{L'R', {Key::LeftShift, Key::R}},
	{L'S', {Key::LeftShift, Key::S}},	{L'T', {Key::LeftShift, Key::T}},	{L'U', {Key::LeftShift, Key::U}},
	{L'V', {Key::LeftShift, Key::V}},	{L'W', {Key::LeftShift, Key::W}},	{L'X', {Key::LeftShift, Key::X}},
	{L'Y', {Key::LeftShift, Key::Y}},	{L'Z', {Key::LeftShift, Key::Z}},

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

const std::vector<uint16_t> *CharacterMapper::sequence_for_character(const wchar_t character) const {
	return lookup_sequence(sequences, character);
}
