//
//  OricMFMDSK.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 21/11/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#include "OricMFMDSK.hpp"
#include "../PCMTrack.hpp"

using namespace Storage::Disk;

OricMFMDSK::OricMFMDSK(const char *file_name) :
	Storage::FileHolder(file_name)
{
	if(!check_signature("MFM_DISK", 8))
		throw ErrorNotOricMFMDSK;

	head_count_ = fgetc32le();
	track_count_ = fgetc32le();
	geometry_type_ = fgetc32le();

	if(geometry_type_ < 1 || geometry_type_ > 2)
		throw ErrorNotOricMFMDSK;
}

unsigned int OricMFMDSK::get_head_position_count()
{
	return track_count_;
}

unsigned int OricMFMDSK::get_head_count()
{
	return head_count_;
}

std::shared_ptr<Track> OricMFMDSK::get_track_at_position(unsigned int head, unsigned int position)
{
	long offset = 0;
	switch(geometry_type_)
	{
		case 1:
			offset = (head * track_count_) + position;
		break;
		case 2:
			offset = (position * track_count_ * head_count_) + head;
		break;
	}
	fseek(file_, (offset * 6400) + 256, SEEK_SET);

	PCMSegment segment;
	segment.number_of_bits = 6250*8;
	segment.data.resize(6250);
	fread(segment.data.data(), 1, 6250, file_);

	std::shared_ptr<PCMTrack> track(new PCMTrack(segment));
	return track;
}
