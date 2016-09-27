//
//  AcornADF.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 25/09/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#include "AcornADF.hpp"

#include <sys/stat.h>
#include "../Encodings/MFM.hpp"

namespace {
	static const unsigned int sectors_per_track = 16;
	static const unsigned int bytes_per_sector = 256;
}

using namespace Storage::Disk;

AcornADF::AcornADF(const char *file_name) : _file(nullptr)
{
	struct stat file_stats;
	stat(file_name, &file_stats);

	// very loose validation: the file needs to be a multiple of 256 bytes
	// and not ungainly large
	if(file_stats.st_size % bytes_per_sector) throw ErrorNotAcornADF;
	if(file_stats.st_size < 7 * bytes_per_sector) throw ErrorNotAcornADF;

	_file = fopen(file_name, "rb");
	if(!_file) throw ErrorCantOpen;

	// check that the initial directory's 'Hugo's are present
	fseek(_file, 513, SEEK_SET);
	uint8_t bytes[4];
	fread(bytes, 1, 4, _file);
	if(bytes[0] != 'H' || bytes[1] != 'u' || bytes[2] != 'g' || bytes[3] != 'o') throw ErrorNotAcornADF;

	fseek(_file, 0x6fb, SEEK_SET);
	fread(bytes, 1, 4, _file);
	if(bytes[0] != 'H' || bytes[1] != 'u' || bytes[2] != 'g' || bytes[3] != 'o') throw ErrorNotAcornADF;
}

AcornADF::~AcornADF()
{
	if(_file) fclose(_file);
}

unsigned int AcornADF::get_head_position_count()
{
	return 80;
}

unsigned int AcornADF::get_head_count()
{
	return 1;
}

std::shared_ptr<Track> AcornADF::get_track_at_position(unsigned int head, unsigned int position)
{
	std::shared_ptr<Track> track;

	if(head >= 2) return track;
	long file_offset = (position * 1 + head) * bytes_per_sector * sectors_per_track;
	fseek(_file, file_offset, SEEK_SET);

	std::vector<Storage::Encodings::MFM::Sector> sectors;
	for(int sector = 0; sector < sectors_per_track; sector++)
	{
		Storage::Encodings::MFM::Sector new_sector;
		new_sector.track = (uint8_t)position;
		new_sector.side = (uint8_t)head;
		new_sector.sector = (uint8_t)sector;

		new_sector.data.resize(bytes_per_sector);
		fread(&new_sector.data[0], 1, bytes_per_sector, _file);
		if(feof(_file))
			break;

		sectors.push_back(std::move(new_sector));
	}

	if(sectors.size()) return Storage::Encodings::MFM::GetMFMTrackWithSectors(sectors);

	return track;
}
