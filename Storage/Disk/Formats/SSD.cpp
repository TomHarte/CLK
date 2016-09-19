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

SSD::SSD(const char *file_name) : _file(nullptr)
{
	struct stat file_stats;
	stat(file_name, &file_stats);

	// very loose validation: the file needs to be a multiple of 256 bytes
	// and not ungainly large

	if(file_stats.st_size & 255) throw ErrorNotSSD;
	if(file_stats.st_size < 512) throw ErrorNotSSD;
	if(file_stats.st_size > 800*256) throw ErrorNotSSD;

	_file = fopen(file_name, "rb");

	if(!_file) throw ErrorCantOpen;

	// this has two heads if the suffix is .dsd, one if it's .ssd
	_head_count = (tolower(file_name[strlen(file_name) - 3]) == 'd') ? 2 : 1;
	_track_count = (unsigned int)(file_stats.st_size / (256 * 10));
	if(_track_count < 40) _track_count = 40;
	else if(_track_count < 80) _track_count = 80;
}

SSD::~SSD()
{
	if(_file) fclose(_file);
}

unsigned int SSD::get_head_position_count()
{
	return _track_count;
}

unsigned int SSD::get_head_count()
{
	return _head_count;
}

std::shared_ptr<Track> SSD::get_track_at_position(unsigned int head, unsigned int position)
{
	std::shared_ptr<Track> track;

	if(head >= _head_count) return track;
	long file_offset = (position * _head_count + head) * 256 * 10;
	fseek(_file, file_offset, SEEK_SET);

	std::vector<Storage::Encodings::MFM::Sector> sectors;
	for(int sector = 0; sector < 10; sector++)
	{
		Storage::Encodings::MFM::Sector new_sector;
		new_sector.track = (uint8_t)position;
		new_sector.side = 0;
		new_sector.sector = (uint8_t)sector;

		new_sector.data.resize(256);
		fread(&new_sector.data[0], 1, 256, _file);
		if(feof(_file)) break;

		sectors.push_back(std::move(new_sector));
	}

	if(sectors.size()) return Storage::Encodings::MFM::GetFMTrackWithSectors(sectors);

	return track;
}
