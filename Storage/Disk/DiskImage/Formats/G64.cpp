//
//  G64.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 10/07/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#include "G64.hpp"

#include <cstring>
#include <vector>

#include "../../Track/PCMTrack.hpp"
#include "../../Encodings/CommodoreGCR.hpp"

using namespace Storage::Disk;

G64::G64(const std::string &file_name) :
		file_(file_name) {
	// read and check the file signature
	if(!file_.check_signature("GCR-1541")) throw ErrorNotG64;

	// check the version number
	int version = file_.get8();
	if(version != 0) throw ErrorUnknownVersion;

	// get the number of tracks and track size
	number_of_tracks_ = file_.get8();
	maximum_track_size_ = file_.get16le();
}

int G64::get_head_position_count() {
	// give at least 84 tracks, to yield the normal geometry but,
	// if there are more, shove them in
	return number_of_tracks_ > 84 ? number_of_tracks_ : 84;
}

std::shared_ptr<Track> G64::get_track_at_position(Track::Address address) {
	std::shared_ptr<Track> resulting_track;

	// if there's definitely no track here, return the empty track
	// (TODO: should be supplying one with an index hole?)
	if(address.position >= number_of_tracks_) return resulting_track;
	if(address.head >= 1) return resulting_track;

	// seek to this track's entry in the track table
	file_.seek(static_cast<long>((address.position * 4) + 0xc), SEEK_SET);

	// read the track offset
	uint32_t track_offset;
	track_offset = file_.get32le();

	// if the track offset is zero, this track doesn't exist, so...
	if(!track_offset) return resulting_track;

	// seek to the track start
	file_.seek(static_cast<long>(track_offset), SEEK_SET);

	// get the real track length
	uint16_t track_length;
	track_length = file_.get16le();

	// grab the byte contents of this track
	std::vector<uint8_t> track_contents(track_length);
	file_.read(&track_contents[0], track_length);

	// seek to this track's entry in the speed zone table
	file_.seek(static_cast<long>((address.position * 4) + 0x15c), SEEK_SET);

	// read the speed zone offsrt
	uint32_t speed_zone_offset;
	speed_zone_offset = file_.get32le();

	// if the speed zone is not constant, create a track based on the whole table; otherwise create one that's constant
	if(speed_zone_offset > 3) {
		// seek to start of speed zone
		file_.seek(static_cast<long>(speed_zone_offset), SEEK_SET);

		uint16_t speed_zone_length = (track_length + 3) >> 2;

		// read the speed zone bytes
		uint8_t speed_zone_contents[speed_zone_length];
		file_.read(speed_zone_contents, speed_zone_length);

		// divide track into appropriately timed PCMSegments
		std::vector<PCMSegment> segments;
		unsigned int current_speed = speed_zone_contents[0] >> 6;
		unsigned int start_byte_in_current_speed = 0;
		for(unsigned int byte = 0; byte < track_length; byte ++) {
			unsigned int byte_speed = speed_zone_contents[byte >> 2] >> (6 - (byte&3)*2);
			if(byte_speed != current_speed || byte == static_cast<uint16_t>(track_length-1)) {
				unsigned int number_of_bytes = byte - start_byte_in_current_speed;

				PCMSegment segment;
				segment.number_of_bits = number_of_bytes * 8;
				segment.length_of_a_bit = Encodings::CommodoreGCR::length_of_a_bit_in_time_zone(current_speed);
				segment.data.resize(number_of_bytes);
				std::memcpy(&segment.data[0], &track_contents[start_byte_in_current_speed], number_of_bytes);
				segments.push_back(std::move(segment));

				current_speed = byte_speed;
				start_byte_in_current_speed = byte;
			}
		}

		resulting_track.reset(new PCMTrack(std::move(segments)));
	} else {
		PCMSegment segment;
		segment.number_of_bits = track_length * 8;
		segment.length_of_a_bit = Encodings::CommodoreGCR::length_of_a_bit_in_time_zone(static_cast<unsigned int>(speed_zone_offset));
		segment.data = std::move(track_contents);

		resulting_track.reset(new PCMTrack(std::move(segment)));
	}

	// TODO: find out whether it's possible for a G64 to supply only a partial track. I don't think it is, which would make the
	// above correct but supposing I'm wrong, the above would produce some incorrectly clocked tracks

	return resulting_track;
}
