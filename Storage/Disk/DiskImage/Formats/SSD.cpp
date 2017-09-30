//
//  SSD.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 18/09/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#include "SSD.hpp"

#include "Utility/ImplicitSectors.hpp"

namespace {
	static const unsigned int sectors_per_track = 10;
	static const size_t bytes_per_sector = 256;
	static const unsigned int sector_size = 1;
}

using namespace Storage::Disk;

SSD::SSD(const char *file_name) :
		Storage::FileHolder(file_name) {
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

unsigned int SSD::get_head_position_count() {
	return track_count_;
}

unsigned int SSD::get_head_count() {
	return head_count_;
}

bool SSD::get_is_read_only() {
	return is_read_only_;
}

long SSD::get_file_offset_for_position(unsigned int head, unsigned int position) {
	return (position * head_count_ + head) * 256 * 10;
}

std::shared_ptr<Track> SSD::get_track_at_position(unsigned int head, unsigned int position) {
	uint8_t sectors[bytes_per_sector*sectors_per_track];

	if(head >= head_count_) return nullptr;
	long file_offset = get_file_offset_for_position(head, position);

	{
		std::lock_guard<std::mutex> lock_guard(file_access_mutex_);
		fseek(file_, file_offset, SEEK_SET);
		fread(sectors, 1, sizeof(sectors), file_);
	}

	return track_for_sectors(sectors, (uint8_t)position, (uint8_t)head, 0, sector_size, false);
}

void SSD::set_track_at_position(unsigned int head, unsigned int position, const std::shared_ptr<Track> &track) {
	uint8_t parsed_track[bytes_per_sector*sectors_per_track];
	decode_sectors(*track, parsed_track, 0, sectors_per_track-1, sector_size, false);

	long file_offset = get_file_offset_for_position(head, position);

	std::lock_guard<std::mutex> lock_guard(file_access_mutex_);
	ensure_file_is_at_least_length(file_offset);
	fseek(file_, file_offset, SEEK_SET);
	fwrite(parsed_track, 1, sizeof(parsed_track), file_);
}
