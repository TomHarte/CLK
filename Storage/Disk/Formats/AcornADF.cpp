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
	flush_updates();
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

long AcornADF::get_file_offset_for_position(unsigned int head, unsigned int position)
{
	return (position * 1 + head) * bytes_per_sector * sectors_per_track;
}

std::shared_ptr<Track> AcornADF::get_uncached_track_at_position(unsigned int head, unsigned int position)
{
	std::shared_ptr<Track> track;

	if(head >= 2) return track;
	long file_offset = get_file_offset_for_position(head, position);
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

void AcornADF::store_updated_track_at_position(unsigned int head, unsigned int position, const std::shared_ptr<Track> &track, std::mutex &file_access_mutex)
{
	std::vector<uint8_t> parsed_track;
	Storage::Encodings::MFM::Parser parser(true, track);
	for(unsigned int c = 0; c < sectors_per_track; c++)
	{
		std::shared_ptr<Storage::Encodings::MFM::Sector> sector = parser.get_sector((uint8_t)position, (uint8_t)c);
		if(sector)
		{
			parsed_track.insert(parsed_track.end(), sector->data.begin(), sector->data.end());
		}
		else
		{
			// TODO: what's correct here? Warn the user that whatever has been written to the disk,
			// it can no longer be stored as an SSD? If so, warn them by what route?
			parsed_track.resize(parsed_track.size() + bytes_per_sector);
		}
	}

	std::lock_guard<std::mutex> lock_guard(file_access_mutex);
	fseek(file_, get_file_offset_for_position(head, position), SEEK_SET);
	fwrite(parsed_track.data(), 1, parsed_track.size(), file_);
}
