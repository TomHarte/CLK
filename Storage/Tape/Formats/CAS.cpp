//
//  CAS.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 25/11/2017.
//  Copyright © 2017 Thomas Harte. All rights reserved.
//

#include "CAS.hpp"

using namespace Storage::Tape;

CAS::CAS(const char *file_name) :
	file_(file_name) {
}

bool CAS::is_at_end() {
	return true;
}

void CAS::virtual_reset() {

}

Tape::Pulse CAS::virtual_get_next_pulse() {
	Pulse empty_pulse;
	return empty_pulse;
}
