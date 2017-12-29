//
//  Typer.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 19/06/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#include "Typer.hpp"

#include <sstream>

using namespace Utility;

Typer::Typer(const std::string &string, HalfCycles delay, HalfCycles frequency, std::unique_ptr<CharacterMapper> character_mapper, Delegate *delegate) :
		frequency_(frequency),
		counter_(-delay),
		delegate_(delegate),
		character_mapper_(std::move(character_mapper)) {
	std::ostringstream string_stream;
	string_stream << Typer::BeginString << string << Typer::EndString;
	string_ = string_stream.str();
}

void Typer::run_for(const HalfCycles duration) {
	if(string_pointer_ < string_.size()) {
		if(counter_ < 0 && counter_ + duration >= 0) {
			if(!type_next_character()) {
				delegate_->typer_reset(this);
			}
		}

		counter_ += duration;
		while(string_pointer_ < string_.size() && counter_ > frequency_) {
			counter_ -= frequency_;
			if(!type_next_character()) {
				delegate_->typer_reset(this);
			}
		}
	}
}

bool Typer::try_type_next_character() {
	uint16_t *sequence = character_mapper_->sequence_for_character(string_[string_pointer_]);

	if(!sequence || sequence[0] == KeyboardMachine::Machine::KeyNotMapped) {
		return false;
	}

	if(!phase_) delegate_->clear_all_keys();
	else {
		delegate_->set_key_state(sequence[phase_ - 1], true);
		return sequence[phase_] != KeyboardMachine::Machine::KeyEndSequence;
	}

	return true;
}

bool Typer::type_next_character() {
	if(string_pointer_ == string_.size()) return false;

	if(!try_type_next_character()) {
		phase_ = 0;
		string_pointer_++;
	} else {
		phase_++;
	}

	return true;
}

// MARK: - Character mapper

uint16_t *CharacterMapper::table_lookup_sequence_for_character(KeySequence *sequences, std::size_t length, char character) {
	std::size_t ucharacter = static_cast<std::size_t>((unsigned char)character);
	if(ucharacter > (length / sizeof(KeySequence))) return nullptr;
	if(sequences[ucharacter][0] == KeyboardMachine::Machine::KeyNotMapped) return nullptr;
	return sequences[ucharacter];
}
