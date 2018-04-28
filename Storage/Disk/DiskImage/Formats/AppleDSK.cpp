//
//  AppleDSK.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 27/04/2018.
//  Copyright © 2018 Thomas Harte. All rights reserved.
//

#include "AppleDSK.hpp"

#include "../../Track/PCMTrack.hpp"
#include "../../Encodings/AppleGCR.hpp"

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
	const std::vector<uint8_t> track_data = file_.read(static_cast<size_t>(bytes_per_sector * sectors_per_track_));

	std::vector<Storage::Disk::PCMSegment> segments;
	const uint8_t track = static_cast<uint8_t>(address.position >> 2);

	// In either case below, the code aims for exactly 50,000 bits per track.
	if(sectors_per_track_ == 16) {
		// Write the sectors.
		for(uint8_t c = 0; c < 16; ++c) {
			segments.push_back(Encodings::AppleGCR::six_and_two_sync(10));
			segments.push_back(Encodings::AppleGCR::header(0, track, c));
			segments.push_back(Encodings::AppleGCR::six_and_two_sync(10));
			segments.push_back(Encodings::AppleGCR::six_and_two_data(&track_data[c * 256]));
			segments.push_back(Encodings::AppleGCR::six_and_two_sync(10));
		}

		// Pad if necessary.
		int encoded_length = (80 + 112 + 80 + 2848 + 80) * sectors_per_track_;
		if(encoded_length < 50000) {
			segments.push_back(Encodings::AppleGCR::six_and_two_sync((50000 - encoded_length) >> 3));
		}
	} else {

	}

	return std::shared_ptr<PCMTrack>(new PCMTrack(segments));
}
