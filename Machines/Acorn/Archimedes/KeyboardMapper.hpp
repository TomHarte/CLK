//
//  KeyboardMapper.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 23/03/2024.
//  Copyright © 2024 Thomas Harte. All rights reserved.
//

#pragma once

#include "../../KeyboardMachine.hpp"

namespace Archimedes {

static constexpr uint16_t map(int row, int column) {
	return static_cast<uint16_t>((row << 4) | column);
}

/// Named key codes that the machine wlll accept directly.
enum Key: uint16_t {
	Escape = map(0, 0),					F1 = map(0, 1),					F2 = map(0, 2),					F3 = map(0, 3),
	F4 = map(0, 4),						F5 = map(0, 5),					F6 = map(0, 6),					F7 = map(0, 7),
	F8 = map(0, 8),						F9 = map(0, 9),					F10 = map(0, 10),				F11 = map(0, 11),
	F12 = map(0, 12),					Print = map(0, 13),				Scroll = map(0, 14),			Break = map(0, 15),

	Tilde = map(1, 0),					k1 = map(1, 1),					k2 = map(1, 2),					k3 = map(1, 3),
	k4 = map(1, 4),						k5 = map(1, 5),					k6 = map(1, 6),					k7 = map(1, 7),
	k8 = map(1, 8),						k9 = map(1, 9),					k0 = map(1, 10),				Hyphen = map(1, 11),
	Equals = map(1, 12),				GBPound = map(1, 13),			Backspace = map(1, 14),			Insert = map(1, 15),

	Home = map(2, 0),					PageUp = map(2, 1),				NumLock = map(2, 2),			KeypadSlash = map(2, 3),
	KeypadAsterisk = map(2, 4),			KeypadHash = map(2, 5),			Tab = map(2, 6),				Q = map(2, 7),
	W = map(2, 8),						E = map(2, 9),					R = map(2, 10),					T = map(2, 11),
	Y = map(2, 12),						U = map(2, 13),					I = map(2, 14),					O = map(2, 15),

	P = map(3, 0),						OpenSquareBracket = map(3, 1),	CloseSquareBracket = map(3, 2),	Backslash = map(3, 3),
	Delete = map(3, 4),					Copy = map(3, 5),				PageDown = map(3, 6),			Keypad7 = map(3, 7),
	Keypad8 = map(3, 8),				Keypad9 = map(3, 9),			KeypadMinus = map(3, 10),		LeftControl = map(3, 11),
	A = map(3, 12),						S = map(3, 13),					D = map(3, 14),					F = map(3, 15),

	G = map(4, 0),						H = map(4, 1),					J = map(4, 2),					K = map(4, 3),
	L = map(4, 4),						Semicolon = map(4, 5),			Quote = map(4, 6),				Return = map(4, 7),
	Keypad4 = map(4, 8),				Keypad5 = map(4, 9),			Keypad6 = map(4, 10),			KeypadPlus = map(4, 11),
	LeftShift = map(4, 12),				/* unused */					Z = map(4, 14),					X = map(4, 14),

	C = map(5, 0),						V = map(5, 1),					B = map(5, 2),					N = map(5, 3),
	M = map(5, 4),						Comma = map(5, 5),				FullStop = map(5, 6),			ForwardSlash = map(5, 7),
	RightShift = map(5, 8),				Up = map(5, 9),					Keypad1 = map(5, 10),			Keypad2 = map(5, 11),
	Keypad3 = map(5, 12),				CapsLock = map(5, 13),			LeftAlt = map(5, 14),			Space = map(5, 15),

	RightAlt = map(6, 0),				RightControl = map(6, 1),		Left = map(6, 2),				Down = map(6, 3),
	Right = map(6, 4),					Keypad0 = map(6, 5),			KeypadDecimalPoint = map(6, 6),	KeypadEnter = map(6, 7),
};

/// Converter from this emulator's custom definition of a generic keyboard to the machine-specific key set defined above.
class KeyboardMapper: public MachineTypes::MappedKeyboardMachine::KeyboardMapper {
	public:
		static constexpr int row(uint16_t key) {
			return key >> 4;
		}

		static constexpr int column(uint16_t key) {
			return key & 0xf;
		}

		// Adapted from the A500 Series Technical Reference Manual.
		uint16_t mapped_key_for_key(Inputs::Keyboard::Key key) const override {
			using k = Inputs::Keyboard::Key;
			switch(key) {
				case k::Escape:				return Key::Escape;
				case k::F1:					return Key::F1;
				case k::F2:					return Key::F2;
				case k::F3:					return Key::F3;
				case k::F4:					return Key::F4;
				case k::F5:					return Key::F5;
				case k::F6:					return Key::F6;
				case k::F7:					return Key::F7;
				case k::F8:					return Key::F8;
				case k::F9:					return Key::F9;
				case k::F10:				return Key::F10;
				case k::F11:				return Key::F11;
				case k::F12:				return Key::F12;
				case k::PrintScreen:		return Key::Print;
				case k::ScrollLock:			return Key::Scroll;
				case k::Pause:				return Key::Break;

				case k::BackTick:			return Key::Tilde;
				case k::k1:					return Key::k1;
				case k::k2:					return Key::k2;
				case k::k3:					return Key::k3;
				case k::k4:					return Key::k4;
				case k::k5:					return Key::k5;
				case k::k6:					return Key::k6;
				case k::k7:					return Key::k7;
				case k::k8:					return Key::k8;
				case k::k9:					return Key::k9;
				case k::k0:					return Key::k0;
				case k::Hyphen:				return Key::Hyphen;
				case k::Equals:				return Key::Equals;
				// TODO: pound key.
				case k::Backspace:			return Key::Backspace;
				case k::Insert:				return Key::Insert;

				case k::Home:				return Key::Home;
				case k::PageUp:				return Key::PageUp;
				case k::NumLock:			return Key::NumLock;
				case k::KeypadSlash:		return Key::KeypadSlash;
				case k::KeypadAsterisk:		return Key::KeypadAsterisk;
				// TODO: keypad hash key
				case k::Tab:				return Key::Tab;
				case k::Q:					return Key::Q;
				case k::W:					return Key::W;
				case k::E:					return Key::E;
				case k::R:					return Key::R;
				case k::T:					return Key::T;
				case k::Y:					return Key::Y;
				case k::U:					return Key::U;
				case k::I:					return Key::I;
				case k::O:					return Key::O;

				case k::P:					return Key::P;
				case k::OpenSquareBracket:	return Key::OpenSquareBracket;
				case k::CloseSquareBracket:	return Key::CloseSquareBracket;
				case k::Backslash:			return Key::Backslash;
				case k::Delete:				return Key::Delete;
				case k::End:				return Key::Copy;
				case k::PageDown:			return Key::PageDown;
				case k::Keypad7:			return Key::Keypad7;
				case k::Keypad8:			return Key::Keypad8;
				case k::Keypad9:			return Key::Keypad9;
				case k::KeypadMinus:		return Key::KeypadMinus;
				case k::LeftControl:		return Key::LeftControl;
				case k::A:					return Key::A;
				case k::S:					return Key::S;
				case k::D:					return Key::D;
				case k::F:					return Key::F;

				case k::G:					return Key::G;
				case k::H:					return Key::H;
				case k::J:					return Key::J;
				case k::K:					return Key::K;
				case k::L:					return Key::L;
				case k::Semicolon:			return Key::Semicolon;
				case k::Quote:				return Key::Quote;
				case k::Enter:				return Key::Return;
				case k::Keypad4:			return Key::Keypad4;
				case k::Keypad5:			return Key::Keypad5;
				case k::Keypad6:			return Key::Keypad6;
				case k::KeypadPlus:			return Key::KeypadPlus;
				case k::LeftShift:			return Key::LeftShift;
				case k::Z:					return Key::Z;
				case k::X:					return Key::X;

				case k::C:					return Key::C;
				case k::V:					return Key::V;
				case k::B:					return Key::B;
				case k::N:					return Key::N;
				case k::M:					return Key::M;
				case k::Comma:				return Key::Comma;
				case k::FullStop:			return Key::FullStop;
				case k::ForwardSlash:		return Key::ForwardSlash;
				case k::RightShift:			return Key::RightShift;
				case k::Up:					return Key::Up;
				case k::Keypad1:			return Key::Keypad1;
				case k::Keypad2:			return Key::Keypad2;
				case k::Keypad3:			return Key::Keypad3;
				case k::CapsLock:			return Key::CapsLock;
				case k::LeftOption:			return Key::LeftAlt;
				case k::Space:				return Key::Space;

				case k::RightOption:		return Key::RightAlt;
				case k::RightControl:		return Key::RightControl;
				case k::Left:				return Key::Left;
				case k::Down:				return Key::Down;
				case k::Right:				return Key::Right;
				case k::Keypad0:			return Key::Keypad0;
				case k::KeypadDecimalPoint:	return Key::KeypadDecimalPoint;
				case k::KeypadEnter:		return Key::KeypadEnter;

				default:	return MachineTypes::MappedKeyboardMachine::KeyNotMapped;
			}
		}
};

}
