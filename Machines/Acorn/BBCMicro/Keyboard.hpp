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

namespace BBCMicro {

enum class Key: uint16_t {
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

	Right = 0x79, 		Left = 0x19, 		Down = 0x29, 		Up = 0x39,
	Return = 0x49, 		Delete = 0x59, 		Copy = 0x69, 		Bit0 = 0x09,

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
	Keypad4 = 0x7a, 		Keypad6 = 0x1a, 		Keypad8 = 0x2a, 	KeypadPlus = 0x3a,
	KeypadDivide = 0x4a, 	KeypadHash = 0x5a,		Keypad0 = 0x6a,
	Keypad5 = 0x7b, 		Keypad7 = 0x1b, 		Keypad9 = 0x2b, 	KeypadMinus = 0x3b,
	KeypadDeleted = 0x4b, 	KeypadMultiply = 0x5b,	Keypad1 = 0x6b,
	Keypad2 = 0x7c, 		F11 = 0x1c, 			PauseBreak = 0x2c, 	KeypadReturn = 0x3c,
	KeypadDot = 0x4c, 		KeypadComma = 0x5c,		Keypad3 = 0x6c,

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

constexpr bool is_modifier(const Key key) {
	return key == Key::Shift || key == Key::Control;
}

struct KeyboardMapper: public MachineTypes::MappedKeyboardMachine::KeyboardMapper {
	uint16_t mapped_key_for_key(const Inputs::Keyboard::Key key) const override {
		const auto found = key_map.find(key);
		return found != key_map.end() ? uint16_t(found->second) : MachineTypes::MappedKeyboardMachine::KeyNotMapped;
	}

private:
	using CLKKey = Inputs::Keyboard::Key;
	static inline const std::unordered_map<CLKKey, Key> key_map{
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
	const uint16_t *sequence_for_character(const char character) const override {
		const auto found = sequences.find(character);
		return found != sequences.end() ? found->second.data() : nullptr;
	}

	bool needs_pause_after_reset_all_keys() const override	{ return false; }
	bool needs_pause_after_key(const uint16_t key) const override {
		return !is_modifier(Key(key));
	}

private:
	static constexpr size_t MaxSequenceLength = 4;
	using Sequence = std::array<uint16_t, MaxSequenceLength>;

	template <size_t n>
	requires (n < MaxSequenceLength - 1)
	static constexpr Sequence keys(const Key (&keys)[n]){
		Sequence sequence;
		for(size_t c = 0; c < n; c++) {
			sequence[c] = uint16_t(keys[c]);
		}
		sequence[n] = MachineTypes::MappedKeyboardMachine::KeyEndSequence;
		return sequence;
	}

	static inline const std::unordered_map<char, Sequence> sequences = {
		{Utility::Typer::BeginString, keys({Key::SwitchOffCaps})},
		{Utility::Typer::EndString, keys({Key::RestoreCaps})},

		{'q', keys({Key::Q}) },	{'w', keys({Key::W}) },
		{'e', keys({Key::E}) },	{'r', keys({Key::R}) },
		{'t', keys({Key::T}) },	{'y', keys({Key::Y}) },
		{'u', keys({Key::U}) },	{'i', keys({Key::I}) },
		{'o', keys({Key::O}) },	{'p', keys({Key::P}) },
		{'a', keys({Key::A}) },	{'s', keys({Key::S}) },
		{'d', keys({Key::D}) },	{'f', keys({Key::F}) },
		{'g', keys({Key::G}) },	{'h', keys({Key::H}) },
		{'j', keys({Key::J}) },	{'k', keys({Key::K}) },
		{'l', keys({Key::L}) },	{'z', keys({Key::Z}) },
		{'x', keys({Key::X}) },	{'c', keys({Key::C}) },
		{'v', keys({Key::V}) },	{'b', keys({Key::B}) },
		{'n', keys({Key::N}) },	{'m', keys({Key::M}) },

		{'Q', keys({Key::Shift, Key::Q}) },	{'W', keys({Key::Shift, Key::W}) },
		{'E', keys({Key::Shift, Key::E}) },	{'R', keys({Key::Shift, Key::R}) },
		{'T', keys({Key::Shift, Key::T}) },	{'Y', keys({Key::Shift, Key::Y}) },
		{'U', keys({Key::Shift, Key::U}) },	{'I', keys({Key::Shift, Key::I}) },
		{'O', keys({Key::Shift, Key::O}) },	{'P', keys({Key::Shift, Key::P}) },
		{'A', keys({Key::Shift, Key::A}) },	{'S', keys({Key::Shift, Key::S}) },
		{'D', keys({Key::Shift, Key::D}) },	{'F', keys({Key::Shift, Key::F}) },
		{'G', keys({Key::Shift, Key::G}) },	{'H', keys({Key::Shift, Key::H}) },
		{'J', keys({Key::Shift, Key::J}) },	{'K', keys({Key::Shift, Key::K}) },
		{'L', keys({Key::Shift, Key::L}) },	{'Z', keys({Key::Shift, Key::Z}) },
		{'X', keys({Key::Shift, Key::X}) },	{'C', keys({Key::Shift, Key::C}) },
		{'V', keys({Key::Shift, Key::V}) },	{'B', keys({Key::Shift, Key::B}) },
		{'N', keys({Key::Shift, Key::N}) },	{'M', keys({Key::Shift, Key::M}) },

		{'0', keys({Key::k0}) },	{'1', keys({Key::k1}) },
		{'2', keys({Key::k2}) },	{'3', keys({Key::k3}) },
		{'4', keys({Key::k4}) },	{'5', keys({Key::k5}) },
		{'6', keys({Key::k6}) },	{'7', keys({Key::k7}) },
		{'8', keys({Key::k8}) },	{'9', keys({Key::k9}) },

		{'\n', keys({Key::Return}) },
		{'\r', keys({Key::Return}) },
		{'\b', keys({Key::Delete}) },
		{'\t', keys({Key::Tab}) },
		{' ', keys({Key::Space}) },

		{'!', keys({Key::Shift, Key::k1}) },
		{'"', keys({Key::Shift, Key::k2}) },
		{'#', keys({Key::Shift, Key::k3}) },
		{'$', keys({Key::Shift, Key::k4}) },
		{'%', keys({Key::Shift, Key::k5}) },
		{'&', keys({Key::Shift, Key::k6}) },
		{'\'', keys({Key::Shift, Key::k7}) },
		{'(', keys({Key::Shift, Key::k8}) },
		{')', keys({Key::Shift, Key::k9}) },

		{'-', keys({Key::Hyphen}) },
		{'^', keys({Key::Caret}) },
		{'\\', keys({Key::Backslash}) },
		{'=', keys({Key::Shift, Key::Hyphen}) },
		{'~', keys({Key::Shift, Key::Caret}) },
		{'|', keys({Key::Shift, Key::Backslash}) },

		{'@', keys({Key::At}) },
		{'[', keys({Key::OpenSquareBracket}) },
		{'_', keys({Key::Underscore}) },
		{'{', keys({Key::Shift, Key::OpenSquareBracket}) },
//		{'£', keys({BBCKey::Shift, BBCKey::Underscore}) },

		{';', keys({Key::Semicolon}) },
		{':', keys({Key::Colon}) },
		{']', keys({Key::CloseSquareBracket}) },
		{'+', keys({Key::Shift, Key::Semicolon}) },
		{'*', keys({Key::Shift, Key::Colon}) },
		{'}', keys({Key::Shift, Key::CloseSquareBracket}) },

		{',', keys({Key::Comma}) },
		{'.', keys({Key::FullStop}) },
		{'/', keys({Key::ForwardSlash}) },
		{'<', keys({Key::Shift, Key::Comma}) },
		{'>', keys({Key::Shift, Key::FullStop}) },
		{'?', keys({Key::Shift, Key::ForwardSlash}) },
	};
};

}
