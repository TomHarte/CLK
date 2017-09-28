//
//  AcornADF.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 25/09/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#include "AcornADF.hpp"

#include <sys/stat.h>
#include "../../Encodings/MFM/Constants.hpp"
#include "../../Encodings/MFM/Encoder.hpp"
#include "../../Track/TrackSerialiser.hpp"
#include "../../Encodings/MFM/SegmentParser.hpp"

namespace {
	static const unsigned int sectors_per_track = 16;
	static const size_t bytes_per_sector = 256;
	static const unsigned int sector_size = 1;
}

using namespace Storage::Disk;

AcornADF::AcornADF(const char *file_name) :
		Storage::FileHolder(file_name) {
	// very loose validation: the file needs to be a multiple of 256 bytes
	// and not ungainly large
	if(file_stats_.st_size % (off_t)bytes_per_sector) throw ErrorNotAcornADF;
	if(file_stats_.st_size < 7 * (off_t)bytes_per_sector) throw ErrorNotAcornADF;

	// check that the initial directory's 'Hugo's are present
	fseek(file_, 513, SEEK_SET);
	uint8_t bytes[4];
	fread(bytes, 1, 4, file_);
	if(bytes[0] != 'H' || bytes[1] != 'u' || bytes[2] != 'g' || bytes[3] != 'o') throw ErrorNotAcornADF;

	fseek(file_, 0x6fb, SEEK_SET);
	fread(bytes, 1, 4, file_);
	if(bytes[0] != 'H' || bytes[1] != 'u' || bytes[2] != 'g' || bytes[3] != 'o') throw ErrorNotAcornADF;
}

unsigned int AcornADF::get_head_position_count() {
	return 80;
}

unsigned int AcornADF::get_head_count() {
	return 1;
}

bool AcornADF::get_is_read_only() {
	return is_read_only_;
}

long AcornADF::get_file_offset_for_position(unsigned int head, unsigned int position) {
	return (position * 1 + head) * bytes_per_sector * sectors_per_track;
}

std::shared_ptr<Track> AcornADF::get_track_at_position(unsigned int head, unsigned int position) {
	std::shared_ptr<Track> track;

	if(head >= 2) return track;
	long file_offset = get_file_offset_for_position(head, position);

	std::vector<Storage::Encodings::MFM::Sector> sectors;
	{
		std::lock_guard<std::mutex> lock_guard(file_access_mutex_);
		fseek(file_, file_offset, SEEK_SET);

		for(unsigned int sector = 0; sector < sectors_per_track; sector++) {
			Storage::Encodings::MFM::Sector new_sector;
			new_sector.address.track = (uint8_t)position;
			new_sector.address.side = (uint8_t)head;
			new_sector.address.sector = (uint8_t)sector;
			new_sector.size = sector_size;

			new_sector.data.resize(bytes_per_sector);
			fread(&new_sector.data[0], 1, bytes_per_sector, file_);
			if(feof(file_))
				break;

			sectors.push_back(std::move(new_sector));
		}
	}

	if(sectors.size()) return Storage::Encodings::MFM::GetMFMTrackWithSectors(sectors);

	return track;
}

void AcornADF::set_track_at_position(unsigned int head, unsigned int position, const std::shared_ptr<Track> &track) {
	std::map<size_t, Storage::Encodings::MFM::Sector> sectors =
		Storage::Encodings::MFM::sectors_from_segment(
			Storage::Disk::track_serialisation(*track, Storage::Encodings::MFM::MFMBitLength),
			true);

	std::vector<uint8_t> parsed_track(sectors_per_track*bytes_per_sector, 0);
	for(auto &pair : sectors) {
		if(pair.second.address.sector >= sectors_per_track) continue;
		if(pair.second.size != sector_size) continue;
		memcpy(&parsed_track[pair.second.address.sector * bytes_per_sector], pair.second.data.data(), std::min(pair.second.data.size(), bytes_per_sector));
	}

	long file_offset = get_file_offset_for_position(head, position);

	std::lock_guard<std::mutex> lock_guard(file_access_mutex_);
	ensure_file_is_at_least_length(file_offset);
	fseek(file_, file_offset, SEEK_SET);
	fwrite(parsed_track.data(), 1, parsed_track.size(), file_);
}
