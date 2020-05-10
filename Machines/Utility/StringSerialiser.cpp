//
//  StringSerialiser.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 13/05/2018.
//  Copyright 2018 Thomas Harte. All rights reserved.
//

#include "StringSerialiser.hpp"

using namespace Utility;

StringSerialiser::StringSerialiser(const std::string &source, bool use_linefeed_only) {
	if(!use_linefeed_only) {
		input_string_ = source;
	} else {
		input_string_.reserve(source.size());

		// Commute any \ns that are not immediately after \rs to \rs; remove the rest.
		bool saw_carriage_return = false;
		for(auto character: source) {
			if(character != '\n') {
				input_string_.push_back(character);
			} else {
				if(!saw_carriage_return) {
					input_string_.push_back('\r');
				}
			}
			saw_carriage_return = character == '\r';
		}
	}
}

uint8_t StringSerialiser::head() {
	if(input_string_pointer_ == input_string_.size())
		return '\0';
	return uint8_t(input_string_[input_string_pointer_]);
}

bool StringSerialiser::advance() {
	if(input_string_pointer_ != input_string_.size()) {
		++input_string_pointer_;
		return input_string_pointer_ != input_string_.size();
	}
	return false;
}
