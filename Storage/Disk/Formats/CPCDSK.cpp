//
//  CPCDSK.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 05/08/2017.
//  Copyright © 2017 Thomas Harte. All rights reserved.
//

#include "CPCDSK.hpp"

using namespace Storage::Disk;

CPCDSK::CPCDSK(const char *file_name) :
	Storage::FileHolder(file_name), is_extended_(false) {
	if(!check_signature("MV - CPC", 8)) {
		is_extended_ = true;
		if(!check_signature("EXTENDED", 8))
			throw ErrorNotCPCDSK;
	}

	// Don't really care about about the creator; skip.
	fseek(file_, 0x30, SEEK_SET);
	head_position_count_ = (unsigned int)fgetc(file_);
	head_count_ = (unsigned int)fgetc(file_);
}

unsigned int CPCDSK::get_head_position_count() {
	return head_position_count_;
}

unsigned int CPCDSK::get_head_count() {
	return head_count_;
}

bool CPCDSK::get_is_read_only() {
	// TODO: allow writing.
	return true;
}

std::shared_ptr<Track> CPCDSK::get_uncached_track_at_position(unsigned int head, unsigned int position) {
	return nullptr;
}
