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
		auto keyboard_machine = machine->keyboard_machine();
		if(keyboard_machine) machines_.push_back(keyboard_machine);
	}
	keyboard_ = std::make_unique<MultiKeyboard>(machines_);
}

void MultiKeyboardMachine::clear_all_keys() {
	for(const auto &machine: machines_) {
		machine->clear_all_keys();
	}
}

void MultiKeyboardMachine::set_key_state(const uint16_t key, const bool is_pressed) {
	for(const auto &machine: machines_) {
		machine->set_key_state(key, is_pressed);
	}
}

void MultiKeyboardMachine::type_string(const std::wstring &string) {
	for(const auto &machine: machines_) {
		machine->type_string(string);
	}
}

bool MultiKeyboardMachine::can_type(const wchar_t c) const {
	bool can_type = true;
	for(const auto &machine: machines_) {
		can_type &= machine->can_type(c);
	}
	return can_type;
}

Inputs::Keyboard &MultiKeyboardMachine::keyboard() {
	return *keyboard_;
}

MultiKeyboardMachine::MultiKeyboard::MultiKeyboard(const std::vector<::MachineTypes::KeyboardMachine *> &machines)
	: machines_(machines) {
	for(const auto &machine: machines_) {
		observed_keys_.insert(
			machine->keyboard().observed_keys().begin(),
			machine->keyboard().observed_keys().end()
		);
		is_exclusive_ |= machine->keyboard().is_exclusive();
	}
}

bool MultiKeyboardMachine::MultiKeyboard::set_key_pressed(
	const Key key,
	const char value,
	const bool is_pressed,
	const bool is_repeat
) {
	bool was_consumed = false;
	for(const auto &machine: machines_) {
		was_consumed |= machine->keyboard().set_key_pressed(key, value, is_pressed, is_repeat);
	}
	return was_consumed;
}

void MultiKeyboardMachine::MultiKeyboard::reset_all_keys() {
	for(const auto &machine: machines_) {
		machine->keyboard().reset_all_keys();
	}
}

const std::set<Inputs::Keyboard::Key> &MultiKeyboardMachine::MultiKeyboard::observed_keys() const {
	return observed_keys_;
}

bool MultiKeyboardMachine::MultiKeyboard::is_exclusive() const {
	return is_exclusive_;
}
