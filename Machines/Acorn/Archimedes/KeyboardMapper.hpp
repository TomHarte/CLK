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

class KeyboardMapper: public MachineTypes::MappedKeyboardMachine::KeyboardMapper {
	public:
		static constexpr uint16_t map(int row, int column) {
			return static_cast<uint16_t>((row << 4) | column);
		}

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
				case k::Escape:			return map(0, 0);
				case k::F1:				return map(0, 1);
				case k::F2:				return map(0, 2);
				case k::F3:				return map(0, 3);
				case k::F4:				return map(0, 4);
				case k::F5:				return map(0, 5);
				case k::F6:				return map(0, 6);
				case k::F7:				return map(0, 7);
				case k::F8:				return map(0, 8);
				case k::F9:				return map(0, 9);
				case k::F10:			return map(0, 10);
				case k::F11:			return map(0, 11);
				case k::F12:			return map(0, 12);
				case k::PrintScreen:	return map(0, 13);
				case k::ScrollLock:		return map(0, 14);
				case k::Pause:			return map(0, 15);

				case k::BackTick:	return map(1, 0);
				case k::k1:			return map(1, 1);
				case k::k2:			return map(1, 2);
				case k::k3:			return map(1, 3);
				case k::k4:			return map(1, 4);
				case k::k5:			return map(1, 5);
				case k::k6:			return map(1, 6);
				case k::k7:			return map(1, 7);
				case k::k8:			return map(1, 8);
				case k::k9:			return map(1, 9);
				case k::k0:			return map(1, 10);
				case k::Hyphen:		return map(1, 11);
				case k::Equals:		return map(1, 12);
				// TODO: pound key.
				case k::Backspace:	return map(1, 14);
				case k::Insert:		return map(1, 15);

				case k::Home:			return map(2, 0);
				case k::PageUp:			return map(2, 1);
				case k::NumLock:		return map(2, 2);
				case k::KeypadSlash:	return map(2, 3);
				case k::KeypadAsterisk:	return map(2, 4);
				// TODO: keypad hash key
				case k::Tab:			return map(2, 6);
				case k::Q:				return map(2, 7);
				case k::W:				return map(2, 8);
				case k::E:				return map(2, 9);
				case k::R:				return map(2, 10);
				case k::T:				return map(2, 11);
				case k::Y:				return map(2, 12);
				case k::U:				return map(2, 13);
				case k::I:				return map(2, 14);
				case k::O:				return map(2, 15);

				case k::P:					return map(3, 0);
				case k::OpenSquareBracket:	return map(3, 1);
				case k::CloseSquareBracket:	return map(3, 2);
				case k::Backslash:			return map(3, 3);
				case k::Delete:				return map(3, 4);
				case k::End:				return map(3, 5);
				case k::PageDown:			return map(3, 6);
				case k::Keypad7:			return map(3, 7);
				case k::Keypad8:			return map(3, 8);
				case k::Keypad9:			return map(3, 9);
				case k::KeypadMinus:		return map(3, 10);
				case k::LeftControl:		return map(3, 11);
				case k::A:					return map(3, 12);
				case k::S:					return map(3, 13);
				case k::D:					return map(3, 14);
				case k::F:					return map(3, 15);

				case k::G:			return map(4, 0);
				case k::H:			return map(4, 1);
				case k::J:			return map(4, 2);
				case k::K:			return map(4, 3);
				case k::L:			return map(4, 4);
				case k::Semicolon:	return map(4, 5);
				case k::Quote:		return map(4, 6);
				case k::Enter:		return map(4, 7);
				case k::Keypad4:	return map(4, 8);
				case k::Keypad5:	return map(4, 9);
				case k::Keypad6:	return map(4, 10);
				case k::KeypadPlus:	return map(4, 11);
				case k::LeftShift:	return map(4, 12);
				case k::Z:			return map(4, 14);
				case k::X:			return map(4, 15);

				case k::C:				return map(5, 0);
				case k::V:				return map(5, 1);
				case k::B:				return map(5, 2);
				case k::N:				return map(5, 3);
				case k::M:				return map(5, 4);
				case k::Comma:			return map(5, 5);
				case k::FullStop:		return map(5, 6);
				case k::ForwardSlash:	return map(5, 7);
				case k::RightShift:		return map(5, 8);
				case k::Up:				return map(5, 9);
				case k::Keypad1:		return map(5, 10);
				case k::Keypad2:		return map(5, 11);
				case k::Keypad3:		return map(5, 12);
				case k::CapsLock:		return map(5, 13);
				case k::LeftOption:		return map(5, 14);
				case k::Space:			return map(5, 15);

				case k::RightOption:		return map(6, 0);
				case k::RightControl:		return map(6, 1);
				case k::Left:				return map(6, 2);
				case k::Down:				return map(6, 3);
				case k::Right:				return map(6, 4);
				case k::Keypad0:			return map(6, 5);
				case k::KeypadDecimalPoint:	return map(6, 6);
				case k::KeypadEnter:		return map(6, 7);

				default:	return MachineTypes::MappedKeyboardMachine::KeyNotMapped;
			}
		}
};

}
