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

AcornADF::AcornADF(const char *file_name) :
	Storage::FileHolder(file_name)
{
	// very loose validation: the file needs to be a multiple of 256 bytes
	// and not ungainly large
	if(file_stats_.st_size % bytes_per_sector) throw ErrorNotAcornADF;
	if(file_stats_.st_size < 7 * bytes_per_sector) throw ErrorNotAcornADF;

	// check that the initial directory's 'Hugo's are present
	fseek(file_, 513, SEEK_SET);
	uint8_t bytes[4];
	fread(bytes, 1, 4, file_);
	if(bytes[0] != 'H' || bytes[1] != 'u' || bytes[2] != 'g' || bytes[3] != 'o') throw ErrorNotAcornADF;

	fseek(file_, 0x6fb, SEEK_SET);
	fread(bytes, 1, 4, file_);
	if(bytes[0] != 'H' || bytes[1] != 'u' || bytes[2] != 'g' || bytes[3] != 'o') throw ErrorNotAcornADF;
}

AcornADF::~AcornADF()
{
	if(get_is_modified())
	{
		for(unsigned int head = 0; head < get_head_count(); head++)
		{
			for(unsigned int track = 0; track < get_head_position_count(); track++)
			{
				std::shared_ptr<Storage::Disk::Track> modified_track = get_modified_track_at_position(head, track);
				if(modified_track)
				{
					Storage::Encodings::MFM::Parser parser(true, modified_track);
					for(unsigned int c = 0; c < sectors_per_track; c++)
					{
						std::shared_ptr<Storage::Encodings::MFM::Sector> sector = parser.get_sector((uint8_t)track, (uint8_t)c);
						printf("Sector %d: %p\n", c, sector.get());
					}
				}
			}
		}
	}
}

unsigned int AcornADF::get_head_position_count()
{
	return 80;
}

unsigned int AcornADF::get_head_count()
{
	return 1;
}

bool AcornADF::get_is_read_only()
{
	return is_read_only_;
}

std::shared_ptr<Track> AcornADF::get_uncached_track_at_position(unsigned int head, unsigned int position)
{
	std::shared_ptr<Track> track;

	if(head >= 2) return track;
	long file_offset = (position * 1 + head) * bytes_per_sector * sectors_per_track;
	fseek(file_, file_offset, SEEK_SET);

	std::vector<Storage::Encodings::MFM::Sector> sectors;
	for(int sector = 0; sector < sectors_per_track; sector++)
	{
		Storage::Encodings::MFM::Sector new_sector;
		new_sector.track = (uint8_t)position;
		new_sector.side = (uint8_t)head;
		new_sector.sector = (uint8_t)sector;

		new_sector.data.resize(bytes_per_sector);
		fread(&new_sector.data[0], 1, bytes_per_sector, file_);
		if(feof(file_))
			break;

		sectors.push_back(std::move(new_sector));
	}

	if(sectors.size()) return Storage::Encodings::MFM::GetMFMTrackWithSectors(sectors);

	return track;
}
