//
//  KeyboardMachine.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 10/10/2017.
//  Copyright © 2017 Thomas Harte. All rights reserved.
//

#include "KeyboardMachine.hpp"

using namespace KeyboardMachine;

Machine::Machine() {
	keyboard_.set_delegate(this);
}

void Machine::keyboard_did_change_key(Inputs::Keyboard *keyboard, Inputs::Keyboard::Key key, bool is_pressed) {
	uint16_t mapped_key = get_keyboard_mapper().mapped_key_for_key(key);
	if(mapped_key != KeyNotMapped) set_key_state(mapped_key, is_pressed);
}

void Machine::reset_all_keys(Inputs::Keyboard *keyboard) {
	// TODO: unify naming.
	clear_all_keys();
}

Inputs::Keyboard &Machine::get_keyboard() {
	return keyboard_;
}
