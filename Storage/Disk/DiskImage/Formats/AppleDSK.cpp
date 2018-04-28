//
//  AppleDSK.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 27/04/2018.
//  Copyright © 2018 Thomas Harte. All rights reserved.
//

#include "AppleDSK.hpp"

using namespace Storage::Disk;

namespace {
	const int number_of_tracks = 35;
	const int bytes_per_sector = 256;
}

AppleDSK::AppleDSK(const std::string &file_name) :
	file_(file_name) {
	if(file_.stats().st_size % number_of_tracks*bytes_per_sector) throw Error::InvalidFormat;

	sectors_per_track_ = static_cast<int>(file_.stats().st_size / (number_of_tracks*bytes_per_sector));
	if(sectors_per_track_ != 13 && sectors_per_track_ != 16) throw Error::InvalidFormat;
}

int AppleDSK::get_head_position_count() {
	return number_of_tracks * 4;
}

std::shared_ptr<Track> AppleDSK::get_track_at_position(Track::Address address) {
	const long file_offset = (address.position >> 2) * bytes_per_sector * sectors_per_track_;
	file_.seek(file_offset, SEEK_SET);

//	std::vector<uint8_t> track_data = file_.read(bytes_per_sector * sectors_per_track_);

	return nullptr;
}
