//
//  HFE.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 17/08/2017.
//  Copyright © 2017 Thomas Harte. All rights reserved.
//

#include "HFE.hpp"

#include "../PCMTrack.hpp"

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

bool HFE::get_is_read_only() {
	return true;
}

std::shared_ptr<Track> HFE::get_uncached_track_at_position(unsigned int head, unsigned int position) {
	// Get track position and length from the lookup table; data is then always interleaved
	// based on an assumption of two heads.
	fseek(file_, track_list_offset_ + position * 4, SEEK_SET);

	long track_offset = (long)fgetc16le() << 9;
	uint16_t track_length = fgetc16le();

	fseek(file_, track_offset, SEEK_SET);
	if(head) fseek(file_, 256, SEEK_CUR);

	PCMSegment segment;
	uint16_t side_length = track_length / 2;
	segment.data.resize(side_length);

	uint16_t c = 0;
	while(c < side_length) {
		uint16_t length = (uint16_t)std::min(256, side_length - c);
		fread(&segment.data[c], 1, length, file_);
		c += length;
		fseek(file_, 256, SEEK_CUR);
	}

	std::shared_ptr<Track> track(new PCMTrack(segment));
	return track;
}
