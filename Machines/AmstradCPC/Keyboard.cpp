//
//  Keyboard.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 10/10/2017.
//  Copyright 2017 Thomas Harte. All rights reserved.
//

#include "Keyboard.hpp"

using namespace AmstradCPC;

uint16_t KeyboardMapper::mapped_key_for_key(const Inputs::Keyboard::Key key) const {
	switch(key) {
		using enum Inputs::Keyboard::Key;
		default: break;

		case BackTick: return Key::Copy;

		case k0: return Key::k0;		case k1: return Key::k1;
		case k2: return Key::k2;		case k3: return Key::k3;
		case k4: return Key::k4;		case k5: return Key::k5;
		case k6: return Key::k6;		case k7: return Key::k7;
		case k8: return Key::k8;		case k9: return Key::k9;

		case Q: return Key::Q;		case W: return Key::W;		case E: return Key::E;		case R: return Key::R;		case T: return Key::T;
		case Y: return Key::Y;		case U: return Key::U;		case I: return Key::I;		case O: return Key::O;		case P: return Key::P;
		case A: return Key::A;		case S: return Key::S;		case D: return Key::D;		case F: return Key::F;		case G: return Key::G;
		case H: return Key::H;		case J: return Key::J;		case K: return Key::K;		case L: return Key::L;
		case Z: return Key::Z;		case X: return Key::X;		case C: return Key::C;		case V: return Key::V;
		case B: return Key::B;		case N: return Key::N;		case M: return Key::M;

		case Escape: return Key::Escape;
		case F1: return Key::F1;	case F2: return Key::F2;	case F3: return Key::F3;	case F4: return Key::F4;	case F5: return Key::F5;
		case F6: return Key::F6;	case F7: return Key::F7;	case F8: return Key::F8;	case F9: return Key::F9;	case F10: return Key::F0;

		case F11: return Key::RightSquareBracket;
		case F12: return Key::Clear;

		case Hyphen: return Key::Minus;		case Equals: return Key::Caret;		case Backspace: return Key::Delete;
		case Tab: return Key::Tab;

		case OpenSquareBracket: return Key::At;
		case CloseSquareBracket: return Key::LeftSquareBracket;
		case Backslash: return Key::BackSlash;

		case CapsLock: return Key::CapsLock;
		case Semicolon: return Key::Colon;
		case Quote: return Key::Semicolon;
		case Hash: return Key::RightSquareBracket;
		case Enter: return Key::Return;

		case LeftShift: return Key::Shift;
		case Comma: return Key::Comma;
		case FullStop: return Key::FullStop;
		case ForwardSlash: return Key::ForwardSlash;
		case RightShift: return Key::Shift;

		case LeftControl: return Key::Control;	case LeftOption: return Key::Control;	case LeftMeta: return Key::Control;
		case Space: return Key::Space;
		case RightMeta: return Key::Control;	case RightOption: return Key::Control;	case RightControl: return Key::Control;

		case Left: return Key::Left;	case Right: return Key::Right;
		case Up: return Key::Up;		case Down: return Key::Down;

		case Keypad0: return Key::F0;
		case Keypad1: return Key::F1;		case Keypad2: return Key::F2;		case Keypad3: return Key::F3;
		case Keypad4: return Key::F4;		case Keypad5: return Key::F5;		case Keypad6: return Key::F6;
		case Keypad7: return Key::F7;		case Keypad8: return Key::F8;		case Keypad9: return Key::F9;
		case KeypadPlus: return Key::Semicolon;
		case KeypadMinus: return Key::Minus;

		case KeypadEnter: return Key::Enter;
		case KeypadDecimalPoint: return Key::FullStop;
		case KeypadEquals: return Key::Minus;
		case KeypadSlash: return Key::ForwardSlash;
		case KeypadAsterisk: return Key::Colon;
		case KeypadDelete: return Key::Delete;
	}
	return MachineTypes::MappedKeyboardMachine::KeyNotMapped;
}

namespace {
const std::unordered_map<wchar_t, const std::vector<uint16_t>> sequences = {
	{L'\b', {Key::Delete}},
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
	{L'_', {Key::Shift, Key::k0}},

	{L'=', {Key::Shift, Key::Minus}},		{L'-', {Key::Minus}},
	{L'£', {Key::Shift, Key::Caret}},		{L'↑', {Key::Caret}},

	{L'|', {Key::Shift, Key::At}},					{L'@', {Key::At}},
	{L'{', {Key::Shift, Key::LeftSquareBracket}},	{L'[', {Key::LeftSquareBracket}},

	{L'*', {Key::Shift, Key::Colon}},				{L':', {Key::Colon}},
	{L'+', {Key::Shift, Key::Semicolon}},			{L';', {Key::Semicolon}},
	{L'}', {Key::Shift, Key::RightSquareBracket}},	{L']', {Key::RightSquareBracket}},

	{L'<', {Key::Shift, Key::Comma}},				{L',', {Key::Comma}},
	{L'>', {Key::Shift, Key::FullStop}},			{L'.', {Key::FullStop}},
	{L'?', {Key::Shift, Key::ForwardSlash}},		{L'/', {Key::ForwardSlash}},
	{L'\\', {Key::BackSlash}},

	{L'0', {Key::k0}},	{L'1', {Key::k1}},	{L'2', {Key::k2}},	{L'3', {Key::k3}},	{L'4', {Key::k4}},
	{L'5', {Key::k5}},	{L'6', {Key::k6}},	{L'7', {Key::k7}},	{L'8', {Key::k8}},	{L'9', {Key::k9}},

	{L'A', {Key::Shift, Key::A}},	{L'B', {Key::Shift, Key::B}},	{L'C', {Key::Shift, Key::C}},
	{L'D', {Key::Shift, Key::D}},	{L'E', {Key::Shift, Key::E}},	{L'F', {Key::Shift, Key::F}},
	{L'G', {Key::Shift, Key::G}},	{L'H', {Key::Shift, Key::H}},	{L'I', {Key::Shift, Key::I}},
	{L'J', {Key::Shift, Key::J}},	{L'K', {Key::Shift, Key::K}},	{L'L', {Key::Shift, Key::L}},
	{L'M', {Key::Shift, Key::M}},	{L'N', {Key::Shift, Key::N}},	{L'O', {Key::Shift, Key::O}},
	{L'P', {Key::Shift, Key::P}},	{L'Q', {Key::Shift, Key::Q}},	{L'R', {Key::Shift, Key::R}},
	{L'S', {Key::Shift, Key::S}},	{L'T', {Key::Shift, Key::T}},	{L'U', {Key::Shift, Key::U}},
	{L'V', {Key::Shift, Key::V}},	{L'W', {Key::Shift, Key::W}},	{L'X', {Key::Shift, Key::X}},
	{L'Y', {Key::Shift, Key::Y}},	{L'Z', {Key::Shift, Key::Z}},

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

bool CharacterMapper::needs_pause_after_key(const uint16_t key) const {
	return key != Key::Control && key != Key::Shift;
}
