//
//  AcornADF.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 25/09/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#include "AcornADF.hpp"

#include "Utility/ImplicitSectors.hpp"

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
	uint8_t sectors[bytes_per_sector*sectors_per_track];

	if(head > 1) return nullptr;
	long file_offset = get_file_offset_for_position(head, position);

	{
		std::lock_guard<std::mutex> lock_guard(file_access_mutex_);
		fseek(file_, file_offset, SEEK_SET);
		fread(sectors, 1, sizeof(sectors), file_);
	}

	return track_for_sectors(sectors, (uint8_t)position, (uint8_t)head, 0, sector_size, true);
}

void AcornADF::set_track_at_position(unsigned int head, unsigned int position, const std::shared_ptr<Track> &track) {
	uint8_t parsed_track[bytes_per_sector*sectors_per_track];
	decode_sectors(*track, parsed_track, 0, sectors_per_track-1, sector_size, true);

	long file_offset = get_file_offset_for_position(head, position);

	std::lock_guard<std::mutex> lock_guard(file_access_mutex_);
	ensure_file_is_at_least_length(file_offset);
	fseek(file_, file_offset, SEEK_SET);
	fwrite(parsed_track, 1, sizeof(parsed_track), file_);
}
