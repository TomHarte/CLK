//
//  MSA.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 03/10/2019.
//  Copyright © 2019 Thomas Harte. All rights reserved.
//

#include "MSA.hpp"

using namespace Storage::Disk;

MSA::MSA(const std::string &file_name) :
	file_(file_name) {
}

std::shared_ptr<::Storage::Disk::Track> MSA::get_track_at_position(::Storage::Disk::Track::Address address) {
	return nullptr;
}

HeadPosition MSA::get_maximum_head_position() {
	return HeadPosition(10);
}
