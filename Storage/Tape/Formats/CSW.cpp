//
//  CSW.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 10/07/2017.
//  Copyright © 2017 Thomas Harte. All rights reserved.
//

#include "CSW.hpp"

using namespace Storage::Tape;

CSW::CSW(const char *file_name) :
	Storage::FileHolder(file_name) {
}

bool CSW::is_at_end() {
	return true;
}

void CSW::virtual_reset() {
}

Tape::Pulse CSW::virtual_get_next_pulse() {
	Tape::Pulse pulse;
	return pulse;
}
