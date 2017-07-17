//
//  TZX.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 16/07/2017.
//  Copyright © 2017 Thomas Harte. All rights reserved.
//

#include "TZX.hpp"

using namespace Storage::Tape;

TZX::TZX(const char *file_name) :
	Storage::FileHolder(file_name),
	is_high_(false),
	is_at_end_(false),
	pulse_pointer_(0) {

	// Check for signature followed by a 0x1a
	char identifier[7];
	char signature[] = "ZXTape!";
	fread(identifier, 1, strlen(signature), file_);
	if(memcmp(identifier, signature, strlen(signature))) throw ErrorNotTZX;
	if(fgetc(file_) != 0x1a) throw ErrorNotTZX;

	// Get version number
	uint8_t major_version = (uint8_t)fgetc(file_);
	uint8_t minor_version = (uint8_t)fgetc(file_);

	// Reject if an incompatible version
	if(major_version != 1 || minor_version > 20)  throw ErrorNotTZX;

	// seed initial block contents
	parse_next_chunk();
}

bool TZX::is_at_end() {
	return true;
}

Tape::Pulse TZX::virtual_get_next_pulse() {
	return Tape::Pulse();
}

void TZX::virtual_reset() {
}

void TZX::parse_next_chunk() {
}
