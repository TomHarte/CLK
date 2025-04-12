//
//  G64.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 10/07/2016.
//  Copyright 2016 Thomas Harte. All rights reserved.
//

#include "G64.hpp"

#include <cstring>
#include <vector>

#include "Storage/Disk/Track/PCMTrack.hpp"
#include "Storage/Disk/Encodings/CommodoreGCR.hpp"

using namespace Storage::Disk;

G64::G64(const std::string &file_name) :
		file_(file_name) {
	// read and check the file signature
	if(!file_.check_signature("GCR-1541")) throw Error::InvalidFormat;

	// check the version number
	int version = file_.get();
	if(version != 0) throw Error::UnknownVersion;

	// get the number of tracks and track size
	number_of_tracks_ = file_.get();
	maximum_track_size_ = file_.get_le<uint16_t>();
}

HeadPosition G64::maximum_head_position() const {
	// give at least 84 tracks, to yield the normal geometry but,
	// if there are more, shove them in
	return HeadPosition(number_of_tracks_ > 84 ? number_of_tracks_ : 84, 2);
}

std::unique_ptr<Track> G64::track_at_position(const Track::Address address) const {
	// seek to this track's entry in the track table
	file_.seek(long((address.position.as_half() * 4) + 0xc), SEEK_SET);

	// read the track offset
	const auto track_offset = file_.get_le<uint32_t>();

	// if the track offset is zero, this track doesn't exist, so...
	if(!track_offset) return nullptr;

	// seek to the track start
	file_.seek(long(track_offset), SEEK_SET);

	// get the real track length
	const auto track_length = file_.get_le<uint16_t>();

	// grab the byte contents of this track
	const std::vector<uint8_t> track_contents = file_.read(track_length);

	// seek to this track's entry in the speed zone table
	file_.seek(long((address.position.as_half() * 4) + 0x15c), SEEK_SET);

	// read the speed zone offsrt
	const auto speed_zone_offset = file_.get_le<uint32_t>();

	// if the speed zone is not constant, create a track based on the whole table; otherwise create one that's constant
	if(speed_zone_offset > 3) {
		// seek to start of speed zone
		file_.seek(long(speed_zone_offset), SEEK_SET);

		// read the speed zone bytes
		const uint16_t speed_zone_length = (track_length + 3) >> 2;
		const auto speed_zone_contents = file_.read(speed_zone_length);

		// divide track into appropriately timed PCMSegments
		std::vector<PCMSegment> segments;
		unsigned int current_speed = speed_zone_contents[0] >> 6;
		unsigned int start_byte_in_current_speed = 0;
		for(unsigned int byte = 0; byte < track_length; byte ++) {
			unsigned int byte_speed = speed_zone_contents[byte >> 2] >> (6 - (byte&3)*2);
			if(byte_speed != current_speed || byte == uint16_t(track_length-1)) {
				unsigned int number_of_bytes = byte - start_byte_in_current_speed;

				PCMSegment segment(
					Encodings::CommodoreGCR::length_of_a_bit_in_time_zone(current_speed),
					number_of_bytes * 8,
					&track_contents[start_byte_in_current_speed]);
				segments.push_back(std::move(segment));

				current_speed = byte_speed;
				start_byte_in_current_speed = byte;
			}
		}

		return std::make_unique<PCMTrack>(std::move(segments));
	} else {
		PCMSegment segment(
			Encodings::CommodoreGCR::length_of_a_bit_in_time_zone(unsigned(speed_zone_offset)),
			track_length * 8,
			track_contents
		);

		return std::make_unique<PCMTrack>(std::move(segment));
	}

	// TODO: find out whether it's possible for a G64 to supply only a partial track. I don't think it is, which
	// would make the above correct but supposing I'm wrong, the above would produce some incorrectly clocked tracks.
}

bool G64::represents(const std::string &name) const {
	return name == file_.name();
}
