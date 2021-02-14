//
//  Keyboard.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 13/02/2021.
//  Copyright © 2021 Thomas Harte. All rights reserved.
//

#include "Keyboard.hpp"

using namespace Apple::ADB;

Keyboard::Keyboard(Bus &bus) : ReactiveDevice(bus, 2) {}

void Keyboard::perform_command(const Command &command) {
	if(command.type == Command::Type::Talk) {
		switch(command.reg) {
			case 0:
				// Post up to two key events, or nothing if there are
				// no events pending.
				//
				// Events are:
				//
				//	b7 = 0 for down 1, for up;
				//	b6–b0: key code (mostly 7-bit ASCII)
//				post_response({0x00, 0x00});
			break;

			case 2:
				/*
					In all cases below: 0 = pressed/on; 1 = released/off.

					b15:	None (reserved)
					b14:	Delete
					b13:	Caps lock
					b12:	Reset
					b11:	Control
					b10:	Shift
					b9:		Option
					b8:		Command

					-- From here onwards, available only on the extended keyboard.

					b7:		Num lock/clear
					b6:		Scroll lock
					b5–3:	None (reserved)
					b2:		Scroll Lock LED
					b1:		Caps Lock LED
					b0:		Num Lock LED
				*/
				post_response({0xff, 0xff});
			break;

			default: break;
		}
		return;
	}
}
