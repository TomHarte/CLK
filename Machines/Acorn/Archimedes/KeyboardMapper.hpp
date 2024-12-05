//
//  KeyboardMapper.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 23/03/2024.
//  Copyright © 2024 Thomas Harte. All rights reserved.
//

#pragma once

#include "../../KeyboardMachine.hpp"
#include "Keyboard.hpp"

namespace Archimedes {

/// Converter from this emulator's custom definition of a generic keyboard to the machine-specific key set defined above.
class KeyboardMapper: public MachineTypes::MappedKeyboardMachine::KeyboardMapper {
public:
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
