//
//  KeyboardMachine.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 10/10/2017.
//  Copyright 2017 Thomas Harte. All rights reserved.
//

#include "KeyboardMachine.hpp"

using namespace MachineTypes;

MachineTypes::MappedKeyboardMachine::MappedKeyboardMachine(const std::set<Inputs::Keyboard::Key> &essential_modifiers) : keyboard_(essential_modifiers) {
	keyboard_.set_delegate(this);
}

bool MappedKeyboardMachine::keyboard_did_change_key(Inputs::Keyboard *keyboard, Inputs::Keyboard::Key key, bool is_pressed) {
	uint16_t mapped_key = get_keyboard_mapper()->mapped_key_for_key(key);
	if(mapped_key == KeyNotMapped) return false;
	set_key_state(mapped_key, is_pressed);
	return true;
}

void MappedKeyboardMachine::reset_all_keys(Inputs::Keyboard *keyboard) {
	// TODO: unify naming.
	clear_all_keys();
}

Inputs::Keyboard &MappedKeyboardMachine::get_keyboard() {
	return keyboard_;
}

void KeyboardMachine::type_string(const std::string &) {
}

MappedKeyboardMachine::KeyboardMapper *MappedKeyboardMachine::get_keyboard_mapper() {
	return nullptr;
}
