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

SSD::~SSD()
{
	if(get_is_modified())
	{
		for(unsigned int head = 0; head < head_count_; head++)
		{
			for(unsigned int track = 0; track < track_count_; track++)
			{
				std::shared_ptr<Storage::Disk::Track> modified_track = get_modified_track_at_position(head, track);
				if(modified_track)
				{
					Storage::Encodings::MFM::Parser parser(false, modified_track);
					for(unsigned int c = 0; c < 10; c++)
					{
						std::shared_ptr<Storage::Encodings::MFM::Sector> sector = parser.get_sector((uint8_t)track, (uint8_t)c);
						printf("Sector %d: %p\n", c, sector.get());
					}
				}
			}
		}
	}
}

unsigned int SSD::get_head_position_count()
{
	return track_count_;
}

unsigned int SSD::get_head_count()
{
	return head_count_;
}

bool SSD::get_is_read_only()
{
	return is_read_only_;
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
		fread(new_sector.data.data(), 1, 256, file_);

		// zero out if this wasn't present in the disk image; it's still appropriate to put a sector
		// on disk because one will have been placed during formatting, but there's no reason to leak
		// information from outside the emulated machine's world
		if(feof(file_)) memset(new_sector.data.data(), 0, 256);

		sectors.push_back(std::move(new_sector));
	}

	if(sectors.size()) return Storage::Encodings::MFM::GetFMTrackWithSectors(sectors);

	return track;
}
