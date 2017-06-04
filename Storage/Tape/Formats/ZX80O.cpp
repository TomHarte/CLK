//
//  ZX80O.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 04/06/2017.
//  Copyright © 2017 Thomas Harte. All rights reserved.
//

#include "ZX80O.hpp"

using namespace Storage::Tape;

ZX80O::ZX80O(const char *file_name) :
	Storage::FileHolder(file_name) {

	// then rewind and start again
	virtual_reset();
}

void ZX80O::virtual_reset() {
	fseek(file_, 0, SEEK_SET);
}

bool ZX80O::is_at_end() {
	return feof(file_);
}

Tape::Pulse ZX80O::virtual_get_next_pulse() {
	Tape::Pulse pulse;

	return pulse;
}
