//
//  Keyboard.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 16/09/2025.
//  Copyright © 2025 Thomas Harte. All rights reserved.
//

#pragma once

#include "Machines/KeyboardMachine.hpp"
#include "Machines/Utility/Typer.hpp"

#include <cstdint>
#include <unordered_map>
#include <vector>

namespace BBCMicro {

namespace Key {
enum Key: uint16_t {
	Escape = 0x70,		Q = 0x10,			F0 = 0x20,			k1 = 0x30,
	CapsLock = 0x40,	ShiftLock = 0x50,	Tab = 0x60,			Shift = 0x00,
	F1 = 0x71,			k3 = 0x11,			W = 0x21,			k2 = 0x31,
	A = 0x41,			S = 0x51,			Z = 0x61,			Control = 0x01,
	F2 = 0x72,			k4 = 0x12,			E = 0x22,			D = 0x32,
	X = 0x42,			C = 0x52,			Space = 0x62,		Bit7 = 0x02,
	F3 = 0x73,			k5 = 0x13,			T = 0x23,			R = 0x33,
	F = 0x43,			G = 0x53,			V = 0x63,			Bit6 = 0x03,
	F5 = 0x74,			F4 = 0x14,			k7 = 0x24,			k6 = 0x34,
	Y = 0x44,			H = 0x54,			B = 0x64,			Bit5 = 0x04,
	F6 = 0x75,			k8 = 0x15,			I = 0x25,			U = 0x35,
	J = 0x45,			N = 0x55,			M = 0x65,			Bit4 = 0x05,
	F8 = 0x76,			F7 = 0x16,			k9 = 0x26,			O = 0x36,
	K = 0x46,			L = 0x56,			Comma = 0x66,		Bit3 = 0x06,
	F9 = 0x77,			Hyphen = 0x17,		k0 = 0x27,			P = 0x37,
	At = 0x47,			Semicolon = 0x57,	FullStop = 0x67,	Bit2 = 0x07,

	Backslash = 0x78,
	Caret = 0x18,
	Underscore = 0x28,
	OpenSquareBracket = 0x38,
	Colon = 0x48,
	CloseSquareBracket = 0x58,
	ForwardSlash = 0x68,
	Bit1 = 0x08,

	Right = 0x79,		Left = 0x19,		Down = 0x29,		Up = 0x39,
	Return = 0x49,		Delete = 0x59,		Copy = 0x69,		Bit0 = 0x09,

	//
	// Break; a key, but not on the keyboard matrix.
	//
	Break = 0xfe00,

	//
	// Fictional keys to aid key entry.
	//
	SwitchOffCaps = 0xfe01,
	RestoreCaps = 0xfe02,

	//
	// Master only keys.
	//
	Keypad4 = 0x7a,			Keypad6 = 0x1a,			Keypad8 = 0x2a,		KeypadPlus = 0x3a,
	KeypadDivide = 0x4a,	KeypadHash = 0x5a,		Keypad0 = 0x6a,
	Keypad5 = 0x7b,			Keypad7 = 0x1b,			Keypad9 = 0x2b,		KeypadMinus = 0x3b,
	KeypadDeleted = 0x4b,	KeypadMultiply = 0x5b,	Keypad1 = 0x6b,
	Keypad2 = 0x7c,			F11 = 0x1c,				PauseBreak = 0x2c,	KeypadReturn = 0x3c,
	KeypadDot = 0x4c,		KeypadComma = 0x5c,		Keypad3 = 0x6c,

	Alt = 0x02,
	LeftShift = 0x03,
	LeftControl = 0x04,
	LeftAlt = 0x05,
	RightShift = 0x06,
	RightControl = 0x07,
	RightAlt = 0x08,
	MouseSelect = 0x09,
	MouseMenu = 0x0a,
	MouseAdjust = 0x0b,
};
}

constexpr bool is_modifier(const Key::Key key) {
	return key == Key::Shift || key == Key::Control;
}

struct KeyboardMapper: public MachineTypes::MappedKeyboardMachine::KeyboardMapper {
	uint16_t mapped_key_for_key(const Inputs::Keyboard::Key key) const override {
		const auto found = key_map.find(key);
		return found != key_map.end() ? uint16_t(found->second) : MachineTypes::MappedKeyboardMachine::KeyNotMapped;
	}

private:
	using CLKKey = Inputs::Keyboard::Key;
	static inline const std::unordered_map<CLKKey, Key::Key> key_map{
		{CLKKey::Escape, Key::Escape},
		{CLKKey::F12, Key::Break},

		// These are all wilfully off-by-one to approximate correct layout.
		{CLKKey::F1, Key::F0},	{CLKKey::F2, Key::F1},	{CLKKey::F3, Key::F2},	{CLKKey::F4, Key::F3},
		{CLKKey::F5, Key::F4},	{CLKKey::F6, Key::F5},	{CLKKey::F7, Key::F6},	{CLKKey::F8, Key::F7},
		{CLKKey::F9, Key::F8},	{CLKKey::F10, Key::F9},

		{CLKKey::Backslash, Key::Backslash},

		{CLKKey::Left, Key::Left},	{CLKKey::Right, Key::Right},	{CLKKey::Up, Key::Up},	{CLKKey::Down, Key::Down},

		{CLKKey::Q, Key::Q},	{CLKKey::W, Key::W},	{CLKKey::E, Key::E},	{CLKKey::R, Key::R},
		{CLKKey::T, Key::T},	{CLKKey::Y, Key::Y},	{CLKKey::U, Key::U},	{CLKKey::I, Key::I},
		{CLKKey::O, Key::O},	{CLKKey::P, Key::P},	{CLKKey::A, Key::A},	{CLKKey::S, Key::S},
		{CLKKey::D, Key::D},	{CLKKey::F, Key::F},	{CLKKey::G, Key::G},	{CLKKey::H, Key::H},
		{CLKKey::J, Key::J},	{CLKKey::K, Key::K},	{CLKKey::L, Key::L},	{CLKKey::Z, Key::Z},
		{CLKKey::X, Key::X},	{CLKKey::C, Key::C},	{CLKKey::V, Key::V},	{CLKKey::B, Key::B},
		{CLKKey::N, Key::N},	{CLKKey::M, Key::M},

		{CLKKey::k0, Key::k0},	{CLKKey::k1, Key::k1},	{CLKKey::k2, Key::k2},	{CLKKey::k3, Key::k3},
		{CLKKey::k4, Key::k4},	{CLKKey::k5, Key::k5},	{CLKKey::k6, Key::k6},	{CLKKey::k7, Key::k7},
		{CLKKey::k8, Key::k8},	{CLKKey::k9, Key::k9},

		{CLKKey::Comma, Key::Comma},
		{CLKKey::FullStop, Key::FullStop},
		{CLKKey::ForwardSlash, Key::ForwardSlash},

		{CLKKey::Hyphen, Key::Hyphen},
		{CLKKey::Equals, Key::Caret},
		{CLKKey::BackTick, Key::Copy},

		{CLKKey::OpenSquareBracket, Key::OpenSquareBracket},
		{CLKKey::CloseSquareBracket, Key::CloseSquareBracket},

		{CLKKey::Semicolon, Key::Semicolon},
		{CLKKey::Quote, Key::Colon},

		{CLKKey::Enter, Key::Return},
		{CLKKey::Backspace, Key::Delete},

		{CLKKey::LeftShift, Key::Shift},		{CLKKey::RightShift, Key::Shift},
		{CLKKey::LeftControl, Key::Control},	{CLKKey::RightControl, Key::Control},
		{CLKKey::LeftOption, Key::CapsLock},	{CLKKey::RightOption, Key::CapsLock},

		{CLKKey::Space, Key::Space},
	};
};

struct CharacterMapper: public ::Utility::CharacterMapper {
	const std::vector<uint16_t> *sequence_for_character(const wchar_t character) const override {
		return lookup_sequence(sequences, character);
	}

	bool needs_pause_after_reset_all_keys() const override	{ return false; }
	bool needs_pause_after_key(const uint16_t key) const override {
		return !is_modifier(Key::Key(key));
	}

private:
	static inline const std::unordered_map<wchar_t, const std::vector<uint16_t>> sequences = {
		{Utility::Typer::BeginString, {Key::SwitchOffCaps}},
		{Utility::Typer::EndString, {Key::RestoreCaps}},

		{L'q', {Key::Q} },	{L'w', {Key::W} },
		{L'e', {Key::E} },	{L'r', {Key::R} },
		{L't', {Key::T} },	{L'y', {Key::Y} },
		{L'u', {Key::U} },	{L'i', {Key::I} },
		{L'o', {Key::O} },	{L'p', {Key::P} },
		{L'a', {Key::A} },	{L's', {Key::S} },
		{L'd', {Key::D} },	{L'f', {Key::F} },
		{L'g', {Key::G} },	{L'h', {Key::H} },
		{L'j', {Key::J} },	{L'k', {Key::K} },
		{L'l', {Key::L} },	{L'z', {Key::Z} },
		{L'x', {Key::X} },	{L'c', {Key::C} },
		{L'v', {Key::V} },	{L'b', {Key::B} },
		{L'n', {Key::N} },	{L'm', {Key::M} },

		{L'Q', {Key::Shift, Key::Q} },	{L'W', {Key::Shift, Key::W} },
		{L'E', {Key::Shift, Key::E} },	{L'R', {Key::Shift, Key::R} },
		{L'T', {Key::Shift, Key::T} },	{L'Y', {Key::Shift, Key::Y} },
		{L'U', {Key::Shift, Key::U} },	{L'I', {Key::Shift, Key::I} },
		{L'O', {Key::Shift, Key::O} },	{L'P', {Key::Shift, Key::P} },
		{L'A', {Key::Shift, Key::A} },	{L'S', {Key::Shift, Key::S} },
		{L'D', {Key::Shift, Key::D} },	{L'F', {Key::Shift, Key::F} },
		{L'G', {Key::Shift, Key::G} },	{L'H', {Key::Shift, Key::H} },
		{L'J', {Key::Shift, Key::J} },	{L'K', {Key::Shift, Key::K} },
		{L'L', {Key::Shift, Key::L} },	{L'Z', {Key::Shift, Key::Z} },
		{L'X', {Key::Shift, Key::X} },	{L'C', {Key::Shift, Key::C} },
		{L'V', {Key::Shift, Key::V} },	{L'B', {Key::Shift, Key::B} },
		{L'N', {Key::Shift, Key::N} },	{L'M', {Key::Shift, Key::M} },

		{L'0', {Key::k0} },	{L'1', {Key::k1} },
		{L'2', {Key::k2} },	{L'3', {Key::k3} },
		{L'4', {Key::k4} },	{L'5', {Key::k5} },
		{L'6', {Key::k6} },	{L'7', {Key::k7} },
		{L'8', {Key::k8} },	{L'9', {Key::k9} },

		{L'\n', {Key::Return} },
		{L'\r', {Key::Return} },
		{L'\b', {Key::Delete} },
		{L'\t', {Key::Tab} },
		{L' ', {Key::Space} },

		{L'!', {Key::Shift, Key::k1} },
		{L'"', {Key::Shift, Key::k2} },
		{L'#', {Key::Shift, Key::k3} },
		{L'$', {Key::Shift, Key::k4} },
		{L'%', {Key::Shift, Key::k5} },
		{L'&', {Key::Shift, Key::k6} },
		{L'\'', {Key::Shift, Key::k7} },
		{L'(', {Key::Shift, Key::k8} },
		{L')', {Key::Shift, Key::k9} },

		{L'-', {Key::Hyphen} },
		{L'^', {Key::Caret} },
		{L'\\', {Key::Backslash} },
		{L'=', {Key::Shift, Key::Hyphen} },
		{L'~', {Key::Shift, Key::Caret} },
		{L'|', {Key::Shift, Key::Backslash} },

		{L'@', {Key::At} },
		{L'[', {Key::OpenSquareBracket} },
		{L'_', {Key::Underscore} },
		{L'{', {Key::Shift, Key::OpenSquareBracket} },
		{L'£', {Key::Shift, Key::Underscore} },

		{L';', {Key::Semicolon} },
		{L':', {Key::Colon} },
		{L']', {Key::CloseSquareBracket} },
		{L'+', {Key::Shift, Key::Semicolon} },
		{L'*', {Key::Shift, Key::Colon} },
		{L'}', {Key::Shift, Key::CloseSquareBracket} },

		{L',', {Key::Comma} },
		{L'.', {Key::FullStop} },
		{L'/', {Key::ForwardSlash} },
		{L'<', {Key::Shift, Key::Comma} },
		{L'>', {Key::Shift, Key::FullStop} },
		{L'?', {Key::Shift, Key::ForwardSlash} },
	};
};

}
