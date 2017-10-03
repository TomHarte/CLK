//
//  HFE.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 17/08/2017.
//  Copyright © 2017 Thomas Harte. All rights reserved.
//

#include "HFE.hpp"

#include "../../Track/PCMTrack.hpp"
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

std::shared_ptr<Track> HFE::get_track_at_position(unsigned int head, unsigned int position) {
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
	segment.number_of_bits = side_length * 8;

	uint16_t c = 0;
	while(c < side_length) {
		uint16_t length = (uint16_t)std::min(256, side_length - c);
		fread(&segment.data[c], 1, length, file_);
		c += length;
		fseek(file_, 256, SEEK_CUR);
	}

	// Flip bytes; HFE's preference is that the least-significant bit
	// is serialised first, but PCMTrack posts the most-significant first.
	Storage::Data::BitReverse::reverse(segment.data);

	std::shared_ptr<Track> track(new PCMTrack(segment));
	return track;
}

