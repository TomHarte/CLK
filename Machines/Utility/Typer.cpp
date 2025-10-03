//
//  Typer.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 19/06/2016.
//  Copyright 2016 Thomas Harte. All rights reserved.
//

#include "Typer.hpp"

using namespace Utility;

Typer::Typer(
	const std::string &string,
	const HalfCycles delay,
	const HalfCycles frequency,
	CharacterMapper &character_mapper,
	Delegate *const delegate
) :
		frequency_(frequency),
		counter_(-delay),
		delegate_(delegate),
		character_mapper_(character_mapper) {
	// Retain only those characters that actually map to something.
	if(sequence_for_character(Typer::BeginString)) {
		string_ += Typer::BeginString;
	}
	if(sequence_for_character(Typer::EndString)) {
		string_ += Typer::EndString;
	}

	append(string);
}

void Typer::run_for(const HalfCycles duration) {
	if(string_pointer_ >= string_.size()) {
		return;
	}

	if(counter_ < 0 && counter_ + duration >= 0) {
		if(!type_next_character()) {
			delegate_->typer_reset(*this);
		}
	}

	counter_ += duration;
	while(string_pointer_ < string_.size() && counter_ > frequency_) {
		counter_ -= frequency_;
		if(!type_next_character()) {
			delegate_->typer_reset(*this);
		}
	}
}

void Typer::append(const std::string &string) {
	// Remove any characters that are already completely done;
	// otherwise things may accumulate here indefinitely.
	// Note that sequence_for_character may seek to look one backwards,
	// so keep 'the character before' if there was one.
	if(string_pointer_ > 1) {
		string_.erase(string_.begin(), string_.begin() + ssize_t(string_pointer_) - 1);
		string_pointer_ = 1;
	}

	// If the final character in the string is not Typer::EndString
	// then this machine doesn't need Begin and End, so don't worry about it.
	ssize_t insertion_position = ssize_t(string_.size());
	if(!string_.empty() && string_.back() == Typer::EndString) --insertion_position;

	string_.reserve(string_.size() + string.size());
	for(const char c : string) {
		if(sequence_for_character(c)) {
			string_.insert(string_.begin() + insertion_position, c);
			++insertion_position;
		}
	}
}

const uint16_t *Typer::sequence_for_character(const char c) const {
	const uint16_t *const sequence = character_mapper_.sequence_for_character(c);
	if(!sequence || sequence[0] == MachineTypes::MappedKeyboardMachine::KeyNotMapped) {
		return nullptr;
	}
	return sequence;
}

uint16_t Typer::try_type_next_character() {
	const uint16_t *const sequence = sequence_for_character(string_[string_pointer_]);

	if(!sequence) {
		return 0;
	}

	// Advance phase.
	++phase_;

	// If this is the start of the output sequence, start with a reset all keys.
	// Then pause if either: (i) the machine requires it; or (ii) this is the same
	// character that was just typed, in which case the gap in presses will need to
	// be clear.
	if(phase_ == 1) {
		delegate_->clear_all_keys();
		if(character_mapper_.needs_pause_after_reset_all_keys() ||
			(string_pointer_ > 0 && string_[string_pointer_ - 1] == string_[string_pointer_])) {
			return MachineTypes::MappedKeyboardMachine::DelaySlot;
		}
		++phase_;
	}

	// Don't forward ::KeyEndSequence.
	const auto next = sequence[phase_ - 2];
	if(next != MachineTypes::MappedKeyboardMachine::KeyEndSequence) {
		delegate_->set_key_state(sequence[phase_ - 2], true);
	}
	return next;
}

bool Typer::type_next_character() {
	if(string_pointer_ == string_.size()) return false;

	while(true) {
		const uint16_t key_pressed = try_type_next_character();

		if(key_pressed == MachineTypes::MappedKeyboardMachine::KeyEndSequence) {
			phase_ = 0;
			++string_pointer_;
			if(string_pointer_ == string_.size()) return false;
			continue;
		}

		if(character_mapper_.needs_pause_after_key(key_pressed)) {
			break;
		}
	}

	return true;
}
