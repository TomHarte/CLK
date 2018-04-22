//
//  NIB.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 21/04/2018.
//  Copyright © 2018 Thomas Harte. All rights reserved.
//

#include "NIB.hpp"

using namespace Storage::Disk;

namespace {

const std::size_t track_length = 6656;
const std::size_t number_of_tracks = 35;

}

NIB::NIB(const std::string &file_name) :
	file_(file_name) {
	// A NIB should be 35 tracks, each 6656 bytes long.
	if(file_.stats().st_size != track_length*number_of_tracks) {
		throw ErrorNotNIB;
	}

	// TODO: all other validation.
}

int NIB::get_head_position_count() {
	return 35;
}

int NIB::get_head_count() {
	return 1;
}

bool NIB::get_is_read_only() {
	return true;
}

std::shared_ptr<::Storage::Disk::Track> NIB::get_track_at_position(::Storage::Disk::Track::Address address) {
	return nullptr;
}
