//
//  Keyboard.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 13/12/2024.
//  Copyright 2024 Thomas Harte. All rights reserved.
//

#pragma once

#include "Machines/KeyboardMachine.hpp"
#include "Machines/Utility/Typer.hpp"

namespace Commodore::Plus4 {

static constexpr uint16_t key(const int line, const int mask) {
	return uint16_t((mask << 3) | line);
}
static constexpr size_t line(const uint16_t key) {
	return key & 7;
}
static constexpr uint8_t mask(const uint16_t key) {
	return uint8_t(key >> 3);
}

enum Key: uint16_t {
	InsDel		= key(0, 0x01),		Return		= key(0, 0x02),
	GBP			= key(0, 0x04),		Help_F7		= key(0, 0x08),
	F1_F4		= key(0, 0x10),		F2_F5		= key(0, 0x20),
	F3_F6		= key(0, 0x40),		At			= key(0, 0x80),

	k3			= key(1, 0x01),		W			= key(1, 0x02),
	A			= key(1, 0x04),		k4			= key(1, 0x08),
	Z			= key(1, 0x10),		S			= key(1, 0x20),
	E			= key(1, 0x40),		Shift		= key(1, 0x80),

	k5			= key(2, 0x01),		R			= key(2, 0x02),
	D			= key(2, 0x04),		k6			= key(2, 0x08),
	C			= key(2, 0x10),		F			= key(2, 0x20),
	T			= key(2, 0x40),		X			= key(2, 0x80),

	k7			= key(3, 0x01),		Y			= key(3, 0x02),
	G			= key(3, 0x04),		k8			= key(3, 0x08),
	B			= key(3, 0x10),		H			= key(3, 0x20),
	U			= key(3, 0x40),		V			= key(3, 0x80),

	k9			= key(4, 0x01),		I			= key(4, 0x02),
	J			= key(4, 0x04),		k0			= key(4, 0x08),
	M			= key(4, 0x10),		K			= key(4, 0x20),
	O			= key(4, 0x40),		N			= key(4, 0x80),

	Down		= key(5, 0x01),		P			= key(5, 0x02),
	L			= key(5, 0x04),		Up			= key(5, 0x08),
	FullStop	= key(5, 0x10),		Colon		= key(5, 0x20),
	Minus		= key(5, 0x40),		Comma		= key(5, 0x80),

	Left		= key(6, 0x01),		Asterisk	= key(6, 0x02),
	Semicolon	= key(6, 0x04),		Right		= key(6, 0x08),
	Escape		= key(6, 0x10),		Equals		= key(6, 0x20),
	Plus		= key(6, 0x40),		Slash		= key(6, 0x80),

	k1			= key(7, 0x01),		Clear_Home	= key(7, 0x02),
	Control		= key(7, 0x04),		k2			= key(7, 0x08),
	Space		= key(7, 0x10),		Commodore	= key(7, 0x20),
	Q			= key(7, 0x40),		Run_Stop	= key(7, 0x80),

	// Bonus virtual keys:
	F4			= 0xfe00,
	F5			= 0xfe01,
	F6			= 0xfe02,
	F7			= 0xfe03,
};

struct KeyboardMapper: public MachineTypes::MappedKeyboardMachine::KeyboardMapper {
	uint16_t mapped_key_for_key(Inputs::Keyboard::Key key) const final;
};

struct CharacterMapper: public ::Utility::CharacterMapper {
	const uint16_t *sequence_for_character(char character) const final;
	bool needs_pause_after_reset_all_keys() const final	{ return false; }
	bool needs_pause_after_key(uint16_t key) const	{
		return key != Key::Shift && key != Key::Commodore && key != Key::Control;
	}
};

}
