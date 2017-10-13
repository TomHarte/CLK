//
//  Typer.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 19/06/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#include "Typer.hpp"
#include <stdlib.h>

using namespace Utility;

Typer::Typer(const char *string, HalfCycles delay, HalfCycles frequency, std::unique_ptr<CharacterMapper> character_mapper, Delegate *delegate) :
		counter_(-delay),
		frequency_(frequency),
		string_pointer_(0),
		delegate_(delegate),
		phase_(0),
		character_mapper_(std::move(character_mapper)) {
	size_t string_size = strlen(string) + 3;
	string_ = (char *)malloc(string_size);
	snprintf(string_, string_size, "%c%s%c", Typer::BeginString, string, Typer::EndString);
}

void Typer::run_for(const HalfCycles duration) {
	if(string_) {
		if(counter_ < 0 && counter_ + duration >= 0) {
			if(!type_next_character()) {
				delegate_->typer_reset(this);
			}
		}

		counter_ += duration;
		while(string_ && counter_ > frequency_) {
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
	if(string_ == nullptr) return false;

	if(!try_type_next_character()) {
		phase_ = 0;
		if(!string_[string_pointer_]) {
			free(string_);
			string_ = nullptr;
			return false;
		}

		string_pointer_++;
	} else {
		phase_++;
	}

	return true;
}

Typer::~Typer() {
	free(string_);
}

#pragma mark - Character mapper

uint16_t *CharacterMapper::table_lookup_sequence_for_character(KeySequence *sequences, size_t length, char character) {
	size_t ucharacter = static_cast<size_t>((unsigned char)character);
	if(ucharacter > (length / sizeof(KeySequence))) return nullptr;
	if(sequences[ucharacter][0] == KeyboardMachine::Machine::KeyNotMapped) return nullptr;
	return sequences[ucharacter];
}
