//
//  PlusTooBIN.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 04/06/2019.
//  Copyright © 2019 Thomas Harte. All rights reserved.
//

#include "PlusTooBIN.hpp"

#include "../../Track/PCMTrack.hpp"
#include "../../Encodings/AppleGCR/Encoder.hpp"

using namespace Storage::Disk;

namespace {
const long sector_size = 1024;
}

PlusTooBIN::PlusTooBIN(const std::string &file_name) :
	file_(file_name) {
	// BIN isn't really meant to be an emulator file format, it's primarily
	// a convenience for the PlusToo Macintosh clone. So validation is
	// fairly light.
	if(file_.stats().st_size != 1638400)
		throw Error::InvalidFormat;
}

HeadPosition PlusTooBIN::get_maximum_head_position() {
	return HeadPosition(80);
}

int PlusTooBIN::get_head_count() {
	return 2;
}

std::shared_ptr<Track> PlusTooBIN::get_track_at_position(Track::Address address) {
	if(address.position >= get_maximum_head_position()) return nullptr;
	if(address.head >= get_head_count()) return nullptr;

	const auto start_position = Encodings::AppleGCR::Macintosh::sectors_in_track(address.position.as_int());
	const long file_offset = long(start_position.start * 2 + address.head * start_position.length) * sector_size;
	file_.seek(file_offset, SEEK_SET);

	const auto track_contents = file_.read(std::size_t(sector_size * start_position.length));

	// Split up the data that comes out per encoded sector, prefixing proper sync bits.
	Storage::Disk::PCMSegment segment;
	for(size_t c = 0; c < size_t(start_position.length); ++c) {
		segment += Storage::Encodings::AppleGCR::six_and_two_sync(5);

		size_t data_start = 0;
		while(track_contents[c*1024 + data_start] == 0xff) ++data_start;
		segment += PCMSegment((1024 - data_start) * 8, &track_contents[c*1024 + data_start]);
	}

	return std::make_shared<PCMTrack>(segment);
}
