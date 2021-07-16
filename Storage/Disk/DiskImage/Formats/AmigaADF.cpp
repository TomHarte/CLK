//
//  AmigaADF.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 16/07/2021.
//  Copyright © 2021 Thomas Harte. All rights reserved.
//

#include "AmigaADF.hpp"


using namespace Storage::Disk;

AmigaADF::AmigaADF(const std::string &file_name) :
		file_(file_name) {
	// Dumb validation only for now: a size check.
	if(file_.stats().st_size != 901120) throw Error::InvalidFormat;
}

HeadPosition AmigaADF::get_maximum_head_position() {
	return HeadPosition(80);
}

int AmigaADF::get_head_count() {
	return 2;
}

std::shared_ptr<Track> AmigaADF::get_track_at_position(Track::Address address) {
	file_.seek(get_file_offset_for_position(address), SEEK_SET);

	// TODO.

	return nullptr;
}

long AmigaADF::get_file_offset_for_position(Track::Address address) {
	return (address.position.as_int() * 2 + address.head) * 512 * 11;
}
