//
//  HFE.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 17/08/2017.
//  Copyright © 2017 Thomas Harte. All rights reserved.
//

#include "HFE.hpp"

using namespace Storage::Disk;

HFE::HFE(const char *file_name) :
		Storage::FileHolder(file_name) {
	throw ErrorNotHFE;
}

HFE::~HFE() {
}

unsigned int HFE::get_head_position_count() {
	return track_count_;
}

unsigned int HFE::get_head_count() {
	return head_count_;
}

bool HFE::get_is_read_only() {
	return true;
}

std::shared_ptr<Track> HFE::get_uncached_track_at_position(unsigned int head, unsigned int position) {
	return nullptr;
}
