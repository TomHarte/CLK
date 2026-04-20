//
//  Keyboard.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 10/10/2017.
//  Copyright 2017 Thomas Harte. All rights reserved.
//

#pragma once

#include "Machines/KeyboardMachine.hpp"
#include "Machines/Utility/Typer.hpp"

namespace AmstradCPC {

namespace Key {
enum Key: uint16_t {
#define Line(l, k1, k2, k3, k4, k5, k6, k7, k8)	\
	k1 = (l << 4) | 0x07,	k2 = (l << 4) | 0x06,	k3 = (l << 4) | 0x05,	k4 = (l << 4) | 0x04,\
	k5 = (l << 4) | 0x03,	k6 = (l << 4) | 0x02,	k7 = (l << 4) | 0x01,	k8 = (l << 4) | 0x00,

	Line(0, FDot,		Enter,			F3,			F6,			F9,					Down,		Right,				Up)
	Line(1, F0,			F2,				F1,			F5,			F8,					F7,			Copy,				Left)
	Line(2, Control,	BackSlash,		Shift,		F4,			RightSquareBracket,	Return,		LeftSquareBracket,	Clear)
	Line(3, FullStop,	ForwardSlash,	Colon,		Semicolon,	P,					At,			Minus,				Caret)
	Line(4, Comma,		M,				K,			L,			I,					O,			k9,					k0)
	Line(5, Space,		N,				J,			H,			Y,					U,			k7,					k8)
	Line(6, V,			B,				F,			G,			T,					R,			k5,					k6)
	Line(7, X,			C,				D,			S,			W,					E,			k3,					k4)
	Line(8, Z,			CapsLock,		A,			Tab,		Q,					Escape,		k2,					k1)
	Line(9, Delete,		Joy1Fire3,		Joy2Fire2,	Joy1Fire1,	Joy1Right,			Joy1Left,	Joy1Down,			Joy1Up)

#undef Line
};
};

struct KeyboardMapper: public MachineTypes::MappedKeyboardMachine::KeyboardMapper {
	uint16_t mapped_key_for_key(Inputs::Keyboard::Key) const override;
};

struct CharacterMapper: public ::Utility::CharacterMapper {
	std::span<const uint16_t> sequence_for_character(wchar_t) const override;

	bool needs_pause_after_reset_all_keys() const override	{ return true; }
	bool needs_pause_after_key(uint16_t) const override;
};

}
