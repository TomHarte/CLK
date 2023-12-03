//
//  KeyboardMapper.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 24/11/2023.
//  Copyright © 2023 Thomas Harte. All rights reserved.
//

#ifndef KeyboardMapper_hpp
#define KeyboardMapper_hpp

#include "../KeyboardMachine.hpp"

namespace PCCompatible {

class KeyboardMapper: public MachineTypes::MappedKeyboardMachine::KeyboardMapper {
	public:
		uint16_t mapped_key_for_key(Inputs::Keyboard::Key key) const override {
			using k = Inputs::Keyboard::Key;
			switch(key) {
				case k::Escape:		return 1;

				case k::k1:			return 2;
				case k::k2:			return 3;
				case k::k3:			return 4;
				case k::k4:			return 5;
				case k::k5:			return 6;
				case k::k6:			return 7;
				case k::k7:			return 8;
				case k::k8:			return 9;
				case k::k9:			return 10;
				case k::k0:			return 11;

				case k::Hyphen:		return 12;
				case k::Equals:		return 13;
				case k::Backspace:	return 14;

				case k::Tab:		return 15;
				case k::Q:			return 16;
				case k::W:			return 17;
				case k::E:			return 18;
				case k::R:			return 19;
				case k::T:			return 20;
				case k::Y:			return 21;
				case k::U:			return 22;
				case k::I:			return 23;
				case k::O:			return 24;
				case k::P:			return 25;

				case k::OpenSquareBracket:	return 26;
				case k::CloseSquareBracket:	return 27;
				case k::Enter:				return 28;

				case k::LeftControl:
				case k::RightControl:		return 29;

				case k::A:			return 30;
				case k::S:			return 31;
				case k::D:			return 32;
				case k::F:			return 33;
				case k::G:			return 34;
				case k::H:			return 35;
				case k::J:			return 36;
				case k::K:			return 37;
				case k::L:			return 38;

				case k::Semicolon:	return 39;
				case k::Quote:		return 40;
				case k::BackTick:	return 41;

				case k::LeftShift:	return 42;
				case k::Backslash:	return 43;

				case k::Z:			return 44;
				case k::X:			return 45;
				case k::C:			return 46;
				case k::V:			return 47;
				case k::B:			return 48;
				case k::N:			return 49;
				case k::M:			return 50;

				case k::Comma:			return 51;
				case k::FullStop:		return 52;
				case k::ForwardSlash:	return 53;
				case k::RightShift:		return 54;

				case k::KeypadAsterisk:	return 55;

				case k::LeftOption:
				case k::RightOption:	return 56;
				case k::Space:			return 57;
				case k::CapsLock:		return 58;

				case k::F1:				return 59;
				case k::F2:				return 60;
				case k::F3:				return 61;
				case k::F4:				return 62;
				case k::F5:				return 63;
				case k::F6:				return 64;
				case k::F7:				return 65;
				case k::F8:				return 66;
				case k::F9:				return 67;
				case k::F10:			return 68;

				case k::NumLock:		return 69;
				case k::ScrollLock:		return 70;

				case k::Keypad7:		return 71;
				case k::Up:
				case k::Keypad8:		return 72;
				case k::Keypad9:		return 73;
				case k::KeypadMinus:	return 74;

				case k::Left:
				case k::Keypad4:		return 75;
				case k::Keypad5:		return 76;
				case k::Right:
				case k::Keypad6:		return 77;
				case k::KeypadPlus:		return 78;

				case k::Keypad1:		return 79;
				case k::Down:
				case k::Keypad2:		return 80;
				case k::Keypad3:		return 81;

				case k::Keypad0:			return 82;
				case k::KeypadDecimalPoint:	return 83;
				/* TODO: SysReq = 84 */

				case k::F11:			return 87;
				case k::F12:			return 88;

				default:	return MachineTypes::MappedKeyboardMachine::KeyNotMapped;
			}
		}
};

}

#endif /* KeyboardMapper_hpp */
