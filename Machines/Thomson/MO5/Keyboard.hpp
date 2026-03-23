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
			default:	return 1;

			using enum Inputs::Keyboard::Key;
			case k4:	return 23;	// 4
			case N:		return 0;
			case J:		return 2;
			case H:		return 3;
			case U:		return 4;
			case Y:		return 5;
			case k7:	return 6;
			case k6:	return 7;
			case Comma:	return 9;
			case K:		return 10;
			case G:		return 11;
			case I:		return 12;
			case T:		return 13;
			case k8:	return 14;
			case k5:	return 15;
			case FullStop:	return 16;
//			case k2:	return 17;	// [go to top left?]
			case L:		return 18;

			case F:		return 19;
			case O:		return 20;
			case R:		return 21;
			case k9:	return 22;
//			case k4:	return 23;	[duplicate?]
//			case k3:	return 24;	// @

			case Space:		return 25;	// [space]
			case M:		return 26;	// M
			case D:		return 27;	// D
			case P:		return 28;	// P
			case E:		return 29;	// E


			case Z:		return 30;	// [space]
			case X:		return 31;	// M
			case C:		return 32;	// D
			case V:		return 33;	// P
			case B:		return 34;	// E
//			case M:		return 35;	// 0
		}

	}
};

}
