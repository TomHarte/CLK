//
//  AppleDSK.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 27/04/2018.
//  Copyright © 2018 Thomas Harte. All rights reserved.
//

#include "AppleDSK.hpp"

#include "../../Track/PCMTrack.hpp"
#include "../../Encodings/AppleGCR/Encoder.hpp"

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

	// Check whether this is a Pro DOS disk by inspecting the filename.
	if(sectors_per_track_ == 16) {
		size_t string_index = file_name.size()-1;
		while(file_name[string_index] != '.') {
			if(file_name[string_index] == 'p') {
				is_prodos_ = true;
				break;
			}
			--string_index;
		}
	}
}

int AppleDSK::get_head_position_count() {
	return number_of_tracks * 4;
}

std::shared_ptr<Track> AppleDSK::get_track_at_position(Track::Address address) {
	const long file_offset = (address.position >> 2) * bytes_per_sector * sectors_per_track_;
	file_.seek(file_offset, SEEK_SET);
	const std::vector<uint8_t> track_data = file_.read(static_cast<size_t>(bytes_per_sector * sectors_per_track_));

	Storage::Disk::PCMSegment segment;
	const uint8_t track = static_cast<uint8_t>(address.position >> 2);

	// In either case below, the code aims for exactly 50,000 bits per track.
	if(sectors_per_track_ == 16) {
		// Write the sectors.
		std::size_t sector_number_ = 0;
		for(uint8_t c = 0; c < 16; ++c) {
			segment += Encodings::AppleGCR::six_and_two_sync(10);
			segment += Encodings::AppleGCR::header(0, track, c);
			segment += Encodings::AppleGCR::six_and_two_sync(10);
			segment += Encodings::AppleGCR::six_and_two_data(&track_data[sector_number_ * 256]);

			// DOS and Pro DOS interleave sectors on disk, and they're represented in a disk
			// image in physical order rather than logical. So that skew needs to be applied here.
			sector_number_ += is_prodos_ ? 8 : 7;
			if(sector_number_ > 0xf) sector_number_ %= 15;
		}

		// Pad if necessary.
		if(segment.number_of_bits < 50000) {
			segment += Encodings::AppleGCR::six_and_two_sync((50000 - segment.number_of_bits) >> 3);
		}
	} else {

	}

	return std::make_shared<PCMTrack>(segment);
}
