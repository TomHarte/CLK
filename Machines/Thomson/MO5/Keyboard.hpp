//
//  Keyboard.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 22/03/2026.
//  Copyright © 2026 Thomas Harte. All rights reserved.
//

#pragma once

#include "Machines/KeyboardMachine.hpp"

namespace MO5 {

enum Key: uint16_t {
};

struct KeyboardMapper: public MachineTypes::MappedKeyboardMachine::KeyboardMapper {
	uint16_t mapped_key_for_key(const Inputs::Keyboard::Key key) const {
		switch(key) {
			default:	return MachineTypes::MappedKeyboardMachine::KeyNotMapped;

			using enum Inputs::Keyboard::Key;
			case k0:	return 0x1e;		case k1:	return 0x2f;
			case k2:	return 0x27;		case k3:	return 0x1f;
			case k4:	return 0x17;		case k5:	return 0x0f;
			case k6:	return 0x07;		case k7:	return 0x06;
			case k8:	return 0x0e;		case k9:	return 0x16;

			case Q:		return 0x2d;		case W:		return 0x25;
			case E:		return 0x1d;		case R:		return 0x15;
			case T:		return 0x0d;		case Y:		return 0x05;
			case U:		return 0x04;		case I:		return 0x0c;
			case O:		return 0x14;		case P:		return 0x1c;
			case A:		return 0x2b;		case S:		return 0x23;
			case D:		return 0x1b;		case F:		return 0x13;
			case G:		return 0x0b;		case H:		return 0x03;
			case J:		return 0x02;		case K:		return 0x0a;
			case L:		return 0x12;		case Z:		return 0x30;
			case X:		return 0x28;		case C:		return 0x32;
			case V:		return 0x2a;		case B:		return 0x22;
			case N:		return 0x00;		case M:		return 0x1a;

			case FullStop:	return 0x10;
			case Comma:		return 0x08;
			case Hyphen:	return 0x26;
			case Equals:	return 0x2e;	// +

			case OpenSquareBracket:	return 0x18;	// @
			case Semicolon:			return 0x1a;	// M

			case CloseSquareBracket:
			case Quote:		return 0x2c;	// Asterisk.

			case Space:		return 0x20;
			case Enter:		return 0x34;
			case Backspace:	return 0x36;
			case Tab:		return 0x37;

			case Up:		return 0x31;	case Down:	return 0x21;
			case Left:		return 0x29;	case Right:	return 0x19;

			case LeftShift:
			case RightShift:	return 0x38;
			case LeftControl:
			case RightControl:	return 0x35;
			case LeftOption:
			case RightOption:	return 0x33;
			case LeftMeta:
			case RightMeta:		return 0x39;

			case Insert:		return 0x09;	// INS.
			case Home:			return 0x11;	// EFF.
		}
	}
};

}
