//
//  OricMFMDSK.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 21/11/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#include "OricMFMDSK.hpp"
#include "../PCMTrack.hpp"
#include "../Encodings/MFM.hpp"

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

std::shared_ptr<Track> OricMFMDSK::get_uncached_track_at_position(unsigned int head, unsigned int position)
{
	long seek_offset = 0;
	switch(geometry_type_)
	{
		case 1:
			seek_offset = (head * track_count_) + position;
		break;
		case 2:
			seek_offset = (position * track_count_ * head_count_) + head;
		break;
	}
	fseek(file_, (seek_offset * 6400) + 256, SEEK_SET);

	PCMSegment segment;

	// The file format omits clock bits. So it's not a genuine MFM capture.
	// A consumer must contextually guess when an FB, FC, etc is meant to be a control mark.
	size_t track_offset = 0;
	uint8_t last_header[6];
	std::unique_ptr<Encodings::MFM::Encoder> encoder = Encodings::MFM::GetMFMEncoder(segment.data);
	while(track_offset < 6250)
	{
		uint8_t next_byte = (uint8_t)fgetc(file_);
		track_offset++;

		switch(next_byte)
		{
			default:
				encoder->add_byte(next_byte);
			break;

			case 0xfe:	// an ID synchronisation
			{
				encoder->add_ID_address_mark();

				for(int byte = 0; byte < 6; byte++)
				{
					last_header[byte] = (uint8_t)fgetc(file_);
					encoder->add_byte(last_header[byte]);
					track_offset++;
					if(track_offset == 6250) break;
				}
			}
			break;

			case 0xfb:	// a data synchronisation
				encoder->add_data_address_mark();

				for(int byte = 0; byte < (128 << last_header[3]) + 2; byte++)
				{
					encoder->add_byte((uint8_t)fgetc(file_));
					track_offset++;
					if(track_offset == 6250) break;
				}
			break;
		}
	}

	segment.number_of_bits = (unsigned int)(segment.data.size() * 8);

	std::shared_ptr<PCMTrack> track(new PCMTrack(segment));
	return track;
}
