//
//  HFE.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 17/08/2017.
//  Copyright © 2017 Thomas Harte. All rights reserved.
//

#include "HFE.hpp"

#include "../../Track/PCMTrack.hpp"
#include "../../Track/TrackSerialiser.hpp"
#include "../../../Data/BitReverse.hpp"

using namespace Storage::Disk;

HFE::HFE(const char *file_name) :
		Storage::FileHolder(file_name) {
	if(!check_signature("HXCPICFE", 8)) throw ErrorNotHFE;

	if(fgetc(file_)) throw ErrorNotHFE;
	track_count_ = (unsigned int)fgetc(file_);
	head_count_ = (unsigned int)fgetc(file_);

	fseek(file_, 7, SEEK_CUR);
	track_list_offset_ = (long)fgetc16le() << 9;
}

HFE::~HFE() {
}

unsigned int HFE::get_head_position_count() {
	return track_count_;
}

unsigned int HFE::get_head_count() {
	return head_count_;
}

/*!
	Seeks to the beginning of the track at @c position underneath @c head,
	returning its length in bytes.

	To read the track, start from the current file position, read 256 bytes,
	skip 256 bytes, read 256 bytes, skip 256 bytes, etc.
*/
uint16_t HFE::seek_track(unsigned int head, unsigned int position) {
	// Get track position and length from the lookup table; data is then always interleaved
	// based on an assumption of two heads.
	fseek(file_, track_list_offset_ + position * 4, SEEK_SET);

	long track_offset = (long)fgetc16le() << 9;
	uint16_t track_length = fgetc16le();

	fseek(file_, track_offset, SEEK_SET);
	if(head) fseek(file_, 256, SEEK_CUR);

	return track_length / 2;
}

std::shared_ptr<Track> HFE::get_track_at_position(unsigned int head, unsigned int position) {
	PCMSegment segment;
	{
		std::lock_guard<std::mutex> lock_guard(file_access_mutex_);
		uint16_t track_length = seek_track(head, position);

		segment.data.resize(track_length);
		segment.number_of_bits = track_length * 8;

		uint16_t c = 0;
		while(c < track_length) {
			uint16_t length = (uint16_t)std::min(256, track_length - c);
			fread(&segment.data[c], 1, length, file_);
			c += length;
			fseek(file_, 256, SEEK_CUR);
		}
	}

	// Flip bytes; HFE's preference is that the least-significant bit
	// is serialised first, but PCMTrack posts the most-significant first.
	Storage::Data::BitReverse::reverse(segment.data);

	std::shared_ptr<Track> track(new PCMTrack(segment));
	return track;
}

void HFE::set_track_at_position(unsigned int head, unsigned int position, const std::shared_ptr<Track> &track) {
	std::unique_lock<std::mutex> lock_guard(file_access_mutex_);
	uint16_t track_length = seek_track(head, position);
	lock_guard.unlock();

	PCMSegment segment = Storage::Disk::track_serialisation(*track, Storage::Time(1, track_length * 8));
	Storage::Data::BitReverse::reverse(segment.data);
	uint16_t data_length = std::min(static_cast<uint16_t>(segment.data.size()), track_length);

	lock_guard.lock();
	seek_track(head, position);

	uint16_t c = 0;
	while(c < data_length) {
		uint16_t length = (uint16_t)std::min(256, data_length - c);
		fwrite(&segment.data[c], 1, length, file_);
		c += length;
		fseek(file_, 256, SEEK_CUR);
	}
	lock_guard.unlock();
}
