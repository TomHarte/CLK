//
//  Keyboard.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 16/09/2025.
//  Copyright © 2025 Thomas Harte. All rights reserved.
//

#pragma once

#include "Machines/KeyboardMachine.hpp"

namespace BBCMicro {

struct KeyboardMapper: public MachineTypes::MappedKeyboardMachine::KeyboardMapper {

uint16_t mapped_key_for_key(const Inputs::Keyboard::Key key) const override {
	using Key = Inputs::Keyboard::Key;
	switch(key) {
		default: break;

		case Key::Escape:	return 0x70;
		case Key::Q:		return 0x10;
		case Key::F10:		return 0x20;
		case Key::k1:		return 0x30;
		case Key::CapsLock:	return 0x40;
//		case Key::CapsLock:	return 0x40;
		case Key::Tab:		return 0x60;
		case Key::LeftShift:
		case Key::RightShift:	return 0x00;

	}

	return MachineTypes::MappedKeyboardMachine::KeyNotMapped;
}

};

}
