//
//  Keyboard.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 06/05/2026.
//  Copyright © 2026 Thomas Harte. All rights reserved.
//

#pragma once

#include "Machines/KeyboardMachine.hpp"
#include "Machines/Utility/Typer.hpp"

#include <cstdint>
#include <span>

namespace Tandy::CoCo::Keyboard {

constexpr uint16_t key(const int column, const int row) {
	return uint16_t((row << 8) | column);
}

constexpr uint8_t column(const uint16_t key) {
	return key & 0xff;
}

constexpr uint8_t row(const uint16_t key) {
	return key >> 8;
}

namespace Key {
enum Key: uint16_t {
	At = key(0, 0),			A = key(0, 1),			B = key(0, 2),			C = key(0, 3),
	D = key(0, 4),			E = key(0, 5),			F = key(0, 6),			G = key(0, 7),

	H = key(1, 0),			I = key(1, 1),			J = key(1, 2),			K = key(1, 3),
	L = key(1, 4),			M = key(1, 5),			N = key(1, 6),			O = key(1, 7),

	P = key(2, 0),			Q = key(2, 1),			R = key(2, 2),			S = key(2, 3),
	T = key(2, 4),			U = key(2, 5),			V = key(2, 6),			W = key(2, 7),

	X = key(3, 0),			Y = key(3, 1),			Z = key(3, 2),			Up = key(3, 3),
	Down = key(3, 4),		Left = key(3, 5),		Right = key(3, 6),		Space = key(3, 7),

	k0 = key(4, 0),			k1 = key(4, 1),			k2 = key(4, 2),			k3 = key(4, 3),
	k4 = key(4, 4),			k5 = key(4, 5),			k6 = key(4, 6),			k7 = key(4, 7),

	k8 = key(5, 0),			k9 = key(5, 1),			Colon = key(5, 2),		Semicolon = key(5, 3),
	Comma = key(5, 4),		Minus = key(5, 5),		FullStop = key(5, 6),	Slash = key(5, 7),

	Enter = key(6, 0),		Clear = key(6, 1),		Break = key(6, 2),		Shift = key(6, 7),
};

static inline std::unordered_map<wchar_t, const std::vector<uint16_t>> sequences = {
	{L'\n', {Enter}},	{L'\r', {Enter}},
	{L' ', {Space}},

	{L'0', {k0}},	{L'1', {k1}},	{L'2', {k2}},	{L'3', {k3}},	{L'4', {k4}},
	{L'5', {k5}},	{L'6', {k6}},	{L'7', {k7}},	{L'8', {k8}},	{L'9', {k9}},

	{L'!', {Shift, k1}},	{L'"', {Shift, k2}},
	{L'#', {Shift, k3}},	{L'$', {Shift, k4}},
	{L'%', {Shift, k5}},	{L'&', {Shift, k6}},
	{L'\'', {Shift, k7}},	{L'(', {Shift, k8}},
	{L')', {Shift, k9}},

	{L'A', {Shift, A}},	{L'B', {Shift, B}},	{L'C', {Shift, C}},
	{L'D', {Shift, D}},	{L'E', {Shift, E}},	{L'F', {Shift, F}},
	{L'G', {Shift, G}},	{L'H', {Shift, H}},	{L'I', {Shift, I}},
	{L'J', {Shift, J}},	{L'K', {Shift, K}},	{L'L', {Shift, L}},
	{L'M', {Shift, M}},	{L'N', {Shift, N}},	{L'O', {Shift, O}},
	{L'P', {Shift, P}},	{L'Q', {Shift, Q}},	{L'R', {Shift, R}},
	{L'S', {Shift, S}},	{L'T', {Shift, T}},	{L'U', {Shift, U}},
	{L'V', {Shift, V}},	{L'W', {Shift, W}},	{L'X', {Shift, X}},
	{L'Y', {Shift, Y}},	{L'Z', {Shift, Z}},

	{L'a', {A}},	{L'b', {B}},	{L'c', {C}},
	{L'd', {D}},	{L'e', {E}},	{L'f', {F}},
	{L'g', {G}},	{L'h', {H}},	{L'i', {I}},
	{L'j', {J}},	{L'k', {K}},	{L'l', {L}},
	{L'm', {M}},	{L'n', {N}},	{L'o', {O}},
	{L'p', {P}},	{L'q', {Q}},	{L'r', {R}},
	{L's', {S}},	{L't', {T}},	{L'u', {U}},
	{L'v', {V}},	{L'w', {W}},	{L'x', {X}},
	{L'y', {Y}},	{L'z', {Z}},

	{L'*', {Shift, Colon}},					{L':', {Colon}},
	{L'=', {Shift, Minus}},					{L'-', {Minus}},
	{L'@', {At}},
	{L'+', {Shift, Semicolon}},				{L';', {Semicolon}},
	{L'<', {Shift, Comma}},					{L',', {Comma}},
	{L'>', {Shift, FullStop}},				{L'.', {FullStop}},
	{L'?', {Shift, Slash}},					{L'/', {Slash}},
};}

struct KeyboardMapper: public MachineTypes::MappedKeyboardMachine::KeyboardMapper {
	uint16_t mapped_key_for_key(const Inputs::Keyboard::Key key) const {
		using In = Inputs::Keyboard::Key;

		switch(key) {
			default:	return MachineTypes::MappedKeyboardMachine::KeyNotMapped;

			case In::k0:	return Key::k0;		case In::k1:	return Key::k1;
			case In::k2:	return Key::k2;		case In::k3:	return Key::k3;
			case In::k4:	return Key::k4;		case In::k5:	return Key::k5;
			case In::k6:	return Key::k6;		case In::k7:	return Key::k7;
			case In::k8:	return Key::k8;		case In::k9:	return Key::k9;

			case In::Q:		return Key::Q;		case In::W:		return Key::W;
			case In::E:		return Key::E;		case In::R:		return Key::R;
			case In::T:		return Key::T;		case In::Y:		return Key::Y;
			case In::U:		return Key::U;		case In::I:		return Key::I;
			case In::O:		return Key::O;		case In::P:		return Key::P;
			case In::A:		return Key::A;		case In::S:		return Key::S;
			case In::D:		return Key::D;		case In::F:		return Key::F;
			case In::G:		return Key::G;		case In::H:		return Key::H;
			case In::J:		return Key::J;		case In::K:		return Key::K;
			case In::L:		return Key::L;		case In::Z:		return Key::Z;
			case In::X:		return Key::X;		case In::C:		return Key::C;
			case In::V:		return Key::V;		case In::B:		return Key::B;
			case In::N:		return Key::N;		case In::M:		return Key::M;

			case In::FullStop:	return Key::FullStop;
			case In::Comma:		return Key::Comma;
			case In::Hyphen:	return Key::Colon;
			case In::Equals:	return Key::Minus;

			case In::OpenSquareBracket:	return Key::Clear;
			case In::Semicolon:			return Key::Semicolon;
			case In::ForwardSlash:		return Key::Slash;

			case In::Space:		return Key::Space;
			case In::Enter:		return Key::Enter;
			case In::Escape:	return Key::Break;

			case In::Up:		return Key::Up;		case In::Down:	return Key::Down;
			case In::Left:		return Key::Left;	case In::Right:	return Key::Right;

			case In::LeftShift:
			case In::RightShift:	return Key::Shift;

			case In::Backspace:		return Key::Left;
		}
	}
};

struct CharacterMapper: public ::Utility::CharacterMapper {
	std::span<const uint16_t> sequence_for_character(const wchar_t character) const final {
		return lookup_sequence(Key::sequences, character);
	}
};
}
