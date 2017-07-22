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

Typer::Typer(const char *string, int delay, int frequency, Delegate *delegate) :
		counter_(-delay), frequency_(frequency), string_pointer_(0), delegate_(delegate), phase_(0) {
	size_t string_size = strlen(string) + 3;
	string_ = (char *)malloc(string_size);
	snprintf(string_, string_size, "%c%s%c", Typer::BeginString, string, Typer::EndString);
}

void Typer::update(int duration) {
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

bool Typer::type_next_character() {
	if(string_ == nullptr) return false;

	if(delegate_->typer_set_next_character(this, string_[string_pointer_], phase_)) {
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

#pragma mark - Delegate

bool Typer::Delegate::typer_set_next_character(Utility::Typer *typer, char character, int phase) {
	uint16_t *sequence = sequence_for_character(typer, character);
	if(!sequence) return true;

	if(!phase) clear_all_keys();
	else {
		set_key_state(sequence[phase - 1], true);
		return sequence[phase] == Typer::Delegate::EndSequence;
	}

	return false;
}

uint16_t *Typer::Delegate::sequence_for_character(Typer *typer, char character) {
	return nullptr;
}

uint16_t *Typer::Delegate::table_lookup_sequence_for_character(KeySequence *sequences, size_t length, char character) {
	size_t ucharacter = (size_t)((unsigned char)character);
	if(ucharacter > (length / sizeof(KeySequence))) return nullptr;
	if(sequences[ucharacter][0] == NotMapped) return nullptr;
	return sequences[ucharacter];
}
