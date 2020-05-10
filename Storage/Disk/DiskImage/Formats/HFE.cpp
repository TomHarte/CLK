//
//  HFE.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 17/08/2017.
//  Copyright 2017 Thomas Harte. All rights reserved.
//

#include "HFE.hpp"

#include "../../Track/PCMTrack.hpp"
#include "../../Track/TrackSerialiser.hpp"
#include "../../../Data/BitReverse.hpp"

using namespace Storage::Disk;

HFE::HFE(const std::string &file_name) :
		file_(file_name) {
	if(!file_.check_signature("HXCPICFE")) throw Error::InvalidFormat;

	if(file_.get8()) throw Error::UnknownVersion;
	track_count_ = file_.get8();
	head_count_ = file_.get8();

	file_.seek(7, SEEK_CUR);
	track_list_offset_ = long(file_.get16le()) << 9;
}

HeadPosition HFE::get_maximum_head_position() {
	return HeadPosition(track_count_);
}

int HFE::get_head_count() {
	return head_count_;
}

/*!
	Seeks to the beginning of the track at @c position underneath @c head,
	returning its length in bytes.

	To read the track, start from the current file position, read 256 bytes,
	skip 256 bytes, read 256 bytes, skip 256 bytes, etc.
*/
uint16_t HFE::seek_track(Track::Address address) {
	// Get track position and length from the lookup table; data is then always interleaved
	// based on an assumption of two heads.
	file_.seek(track_list_offset_ + address.position.as_int() * 4, SEEK_SET);

	long track_offset = long(file_.get16le()) << 9;		// Track offset, in units of 512 bytes.
	uint16_t track_length = file_.get16le();			// Track length, in bytes, containing both the front and back track.

	file_.seek(track_offset, SEEK_SET);
	if(address.head) file_.seek(256, SEEK_CUR);

	return track_length / 2;	// Divide by two to give the track length for a single side.
}

std::shared_ptr<Track> HFE::get_track_at_position(Track::Address address) {
	PCMSegment segment;
	{
		std::lock_guard<std::mutex> lock_guard(file_.get_file_access_mutex());
		uint16_t track_length = seek_track(address);

		segment.data.resize(track_length * 8);

		// HFE tracks are stored as 256 bytes for side 1, then 256 bytes for side 2,
		// then 256 bytes for side 1, then 256 bytes for side 2, etc, until the final
		// 512-byte segment which will contain less than the full 256 bytes.
		//
		// seek_track will have advanced an extra initial 256 bytes if the address
		// refers to side 2, so the loop below can act ass though it were definitely
		// dealing with side 1.
		uint16_t c = 0;
		while(c < track_length) {
			// Decide how many bytes of at most 256 to read, and read them.
			uint16_t length = uint16_t(std::min(256, track_length - c));
			std::vector<uint8_t> section = file_.read(length);

			// Push those into the PCMSegment. In HFE the least-significant bit is
			// serialised first. TODO: move this logic to PCMSegment.
			for(uint16_t byte = 0; byte < length; ++byte) {
				const size_t base = size_t(c + byte) << 3;
				segment.data[base + 0] = !!(section[byte] & 0x01);
				segment.data[base + 1] = !!(section[byte] & 0x02);
				segment.data[base + 2] = !!(section[byte] & 0x04);
				segment.data[base + 3] = !!(section[byte] & 0x08);
				segment.data[base + 4] = !!(section[byte] & 0x10);
				segment.data[base + 5] = !!(section[byte] & 0x20);
				segment.data[base + 6] = !!(section[byte] & 0x40);
				segment.data[base + 7] = !!(section[byte] & 0x80);
			}

			// Advance the target pointer, and skip the next 256 bytes of the file
			// (which will be for the other side of the disk).
			c += length;
			file_.seek(256, SEEK_CUR);
		}
	}

	return std::make_shared<PCMTrack>(segment);
}

void HFE::set_tracks(const std::map<Track::Address, std::shared_ptr<Track>> &tracks) {
	for(auto &track : tracks) {
		std::unique_lock<std::mutex> lock_guard(file_.get_file_access_mutex());
		uint16_t track_length = seek_track(track.first);
		lock_guard.unlock();

		const PCMSegment segment = Storage::Disk::track_serialisation(*track.second, Storage::Time(1, track_length * 8));

		// Convert the segment into a byte encoding, LSB first.
		std::vector<uint8_t> byte_segment = segment.byte_data(false);
		uint16_t data_length = std::min(uint16_t(byte_segment.size()), track_length);

		lock_guard.lock();
		seek_track(track.first);

		uint16_t c = 0;
		while(c < data_length) {
			uint16_t length = uint16_t(std::min(256, data_length - c));
			file_.write(&byte_segment[c], length);
			c += length;
			file_.seek(256, SEEK_CUR);
		}
		lock_guard.unlock();
	}
}

bool HFE::get_is_read_only() {
	return file_.get_is_known_read_only();
}
