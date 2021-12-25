//
//  IPF.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 25/12/2021.
//  Copyright © 2021 Thomas Harte. All rights reserved.
//

#include "IPF.hpp"

using namespace Storage::Disk;


IPF::IPF(const std::string &file_name) : file_(file_name) {
}

HeadPosition IPF::get_maximum_head_position() {
	return HeadPosition(80); // TODO;
}

int IPF::get_head_count() {
	return 2; // TODO;
}

std::shared_ptr<Track> IPF::get_track_at_position([[maybe_unused]] Track::Address address) {
	return nullptr;
}
