//
//  MultiKeyboardMachine.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 09/02/2018.
//  Copyright 2018 Thomas Harte. All rights reserved.
//

#include "MultiKeyboardMachine.hpp"

using namespace Analyser::Dynamic;

MultiKeyboardMachine::MultiKeyboardMachine(const std::vector<std::unique_ptr<::Machine::DynamicMachine>> &machines) {
	for(const auto &machine: machines) {
		KeyboardMachine::Machine *keyboard_machine = machine->keyboard_machine();
		if(keyboard_machine) machines_.push_back(keyboard_machine);
	}
}

void MultiKeyboardMachine::clear_all_keys() {
	for(const auto &machine: machines_) {
		machine->clear_all_keys();
	}
}

void MultiKeyboardMachine::set_key_state(uint16_t key, bool is_pressed) {
	for(const auto &machine: machines_) {
		machine->set_key_state(key, is_pressed);
	}
}

void MultiKeyboardMachine::type_string(const std::string &string) {
	for(const auto &machine: machines_) {
		machine->type_string(string);
	}
}

Inputs::Keyboard &MultiKeyboardMachine::get_keyboard() {
	return keyboard_;
}

//void MultiKeyboardMachine::keyboard_did_change_key(Inputs::Keyboard *keyboard, Inputs::Keyboard::Key key, bool is_pressed) {
//	for(const auto &machine: machines_) {
//		uint16_t mapped_key = machine->get_keyboard_mapper()->mapped_key_for_key(key);
//		if(mapped_key != KeyNotMapped) machine->set_key_state(mapped_key, is_pressed);
//	}
//}
