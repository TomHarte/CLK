//
//  Keyboard.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 10/9/17.
//  Copyright 2017 Thomas Harte. All rights reserved.
//

#include "Keyboard.hpp"

using namespace Inputs;

Keyboard::Keyboard(const std::set<Key> &essential_modifiers) : essential_modifiers_(essential_modifiers) {
	for(int k = 0; k < int(Key::Help); ++k) {
		observed_keys_.insert(Key(k));
	}
}

Keyboard::Keyboard(const std::set<Key> &observed_keys, const std::set<Key> &essential_modifiers) :
	observed_keys_(observed_keys), essential_modifiers_(essential_modifiers), is_exclusive_(false) {}

bool Keyboard::set_key_pressed(Key key, char value, bool is_pressed) {
	std::size_t key_offset = size_t(key);
	if(key_offset >= key_states_.size()) {
		key_states_.resize(key_offset+1, false);
	}
	key_states_[key_offset] = is_pressed;

	if(delegate_) return delegate_->keyboard_did_change_key(this, key, is_pressed);
	return false;
}

const std::set<Inputs::Keyboard::Key> &Keyboard::get_essential_modifiers() {
	return essential_modifiers_;
}

void Keyboard::reset_all_keys() {
	std::fill(key_states_.begin(), key_states_.end(), false);
	if(delegate_) delegate_->reset_all_keys(this);
}

void Keyboard::set_delegate(Delegate *delegate) {
	delegate_ = delegate;
}

bool Keyboard::get_key_state(Key key) {
	std::size_t key_offset = size_t(key);
	if(key_offset >= key_states_.size()) return false;
	return key_states_[key_offset];
}

const std::set<Keyboard::Key> &Keyboard::observed_keys() {
	return observed_keys_;
}

bool Keyboard::is_exclusive() {
	return is_exclusive_;
}
