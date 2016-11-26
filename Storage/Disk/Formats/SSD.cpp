//
//  SSD.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 18/09/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#include "SSD.hpp"

#include <sys/stat.h>
#include "../Encodings/MFM.hpp"

using namespace Storage::Disk;

SSD::SSD(const char *file_name) :
	Storage::FileHolder(file_name)
{
	// very loose validation: the file needs to be a multiple of 256 bytes
	// and not ungainly large

	if(file_stats_.st_size & 255) throw ErrorNotSSD;
	if(file_stats_.st_size < 512) throw ErrorNotSSD;
	if(file_stats_.st_size > 800*256) throw ErrorNotSSD;

	// this has two heads if the suffix is .dsd, one if it's .ssd
	head_count_ = (tolower(file_name[strlen(file_name) - 3]) == 'd') ? 2 : 1;
	track_count_ = (unsigned int)(file_stats_.st_size / (256 * 10));
	if(track_count_ < 40) track_count_ = 40;
	else if(track_count_ < 80) track_count_ = 80;
}

unsigned int SSD::get_head_position_count()
{
	return track_count_;
}

unsigned int SSD::get_head_count()
{
	return head_count_;
}

std::shared_ptr<Track> SSD::get_uncached_track_at_position(unsigned int head, unsigned int position)
{
	std::shared_ptr<Track> track;

	if(head >= head_count_) return track;
	long file_offset = (position * head_count_ + head) * 256 * 10;
	fseek(file_, file_offset, SEEK_SET);

	std::vector<Storage::Encodings::MFM::Sector> sectors;
	for(int sector = 0; sector < 10; sector++)
	{
		Storage::Encodings::MFM::Sector new_sector;
		new_sector.track = (uint8_t)position;
		new_sector.side = 0;
		new_sector.sector = (uint8_t)sector;

		new_sector.data.resize(256);
		fread(&new_sector.data[0], 1, 256, file_);
		if(feof(file_))
			break;

		sectors.push_back(std::move(new_sector));
	}

	if(sectors.size()) return Storage::Encodings::MFM::GetFMTrackWithSectors(sectors);

	return track;
}
