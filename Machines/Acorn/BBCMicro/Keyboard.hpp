//
//  Keyboard.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 16/09/2025.
//  Copyright © 2025 Thomas Harte. All rights reserved.
//

#pragma once

#include "Machines/KeyboardMachine.hpp"

#include <cstdint>
#include <unordered_map>

namespace BBCMicro {

enum class BBCKey: uint16_t {
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
	ForwardSlash = 0x58,
	Bit1 = 0x08,

	Right = 0x79, 		Left = 0x19, 		Down = 0x29, 		Up = 0x39,
	Return = 0x49, 		Delete = 0x59, 		Copy = 0x69, 		Bit0 = 0x09,

	//
	// Break; a key, but not on the keyboard matrix.
	//
	Break = 0xfffd,

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

struct KeyboardMapper: public MachineTypes::MappedKeyboardMachine::KeyboardMapper {

	uint16_t mapped_key_for_key(const Inputs::Keyboard::Key key) const override {
		const auto found = key_map.find(key);
		return found != key_map.end() ? uint16_t(found->second) : MachineTypes::MappedKeyboardMachine::KeyNotMapped;
	}

private:

	using Key = Inputs::Keyboard::Key;
	static inline const std::unordered_map<Key, BBCKey> key_map{
		{Key::Escape, BBCKey::Escape},
		{Key::F12, BBCKey::Break},

		// These are all wilfully off-by-one to approximate correct layout.
		{Key::F1, BBCKey::F0},	{Key::F2, BBCKey::F1},	{Key::F3, BBCKey::F2},	{Key::F4, BBCKey::F3},
		{Key::F5, BBCKey::F4},	{Key::F6, BBCKey::F5},	{Key::F7, BBCKey::F6},	{Key::F8, BBCKey::F7},
		{Key::F9, BBCKey::F8},	{Key::F10, BBCKey::F9},

		{Key::Backslash, BBCKey::Backslash},

		{Key::Left, BBCKey::Left},	{Key::Right, BBCKey::Right},	{Key::Up, BBCKey::Up},	{Key::Down, BBCKey::Down},

		{Key::Q, BBCKey::Q},	{Key::W, BBCKey::W},	{Key::E, BBCKey::E},	{Key::R, BBCKey::R},
		{Key::T, BBCKey::T},	{Key::Y, BBCKey::Y},	{Key::U, BBCKey::U},	{Key::I, BBCKey::I},
		{Key::O, BBCKey::O},	{Key::P, BBCKey::P},	{Key::A, BBCKey::A},	{Key::S, BBCKey::S},
		{Key::D, BBCKey::D},	{Key::F, BBCKey::F},	{Key::G, BBCKey::G},	{Key::H, BBCKey::H},
		{Key::J, BBCKey::J},	{Key::K, BBCKey::K},	{Key::L, BBCKey::L},	{Key::Z, BBCKey::Z},
		{Key::X, BBCKey::X},	{Key::C, BBCKey::C},	{Key::V, BBCKey::V},	{Key::B, BBCKey::B},
		{Key::N, BBCKey::N},	{Key::M, BBCKey::M},

		{Key::k0, BBCKey::k0},	{Key::k1, BBCKey::k1},	{Key::k2, BBCKey::k2},	{Key::k3, BBCKey::k3},
		{Key::k4, BBCKey::k4},	{Key::k5, BBCKey::k5},	{Key::k6, BBCKey::k6},	{Key::k7, BBCKey::k7},
		{Key::k8, BBCKey::k8},	{Key::k9, BBCKey::k9},

		{Key::Comma, BBCKey::Comma},
		{Key::FullStop, BBCKey::FullStop},
		{Key::ForwardSlash, BBCKey::ForwardSlash},

		{Key::Hyphen, BBCKey::Hyphen},
		{Key::Equals, BBCKey::Caret},
		{Key::BackTick, BBCKey::Copy},

		{Key::OpenSquareBracket, BBCKey::OpenSquareBracket},
		{Key::CloseSquareBracket, BBCKey::CloseSquareBracket},

		{Key::Semicolon, BBCKey::Semicolon},
		{Key::Quote, BBCKey::Colon},

		{Key::Enter, BBCKey::Return},
		{Key::Backspace, BBCKey::Delete},

		{Key::LeftShift, BBCKey::Shift},		{Key::RightShift, BBCKey::Shift},
		{Key::LeftControl, BBCKey::Control},	{Key::RightControl, BBCKey::Control},
		{Key::LeftOption, BBCKey::CapsLock},	{Key::RightOption, BBCKey::CapsLock},

		{Key::Space, BBCKey::Space},
	};
};

}
