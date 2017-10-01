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
	static const unsigned int sector_size = 1;
}

using namespace Storage::Disk;

SSD::SSD(const char *file_name) : MFMSectorDump(file_name) {
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

	set_geometry(sectors_per_track, sector_size, false);
}

unsigned int SSD::get_head_position_count() {
	return track_count_;
}

unsigned int SSD::get_head_count() {
	return head_count_;
}

long SSD::get_file_offset_for_position(unsigned int head, unsigned int position) {
	return (position * head_count_ + head) * 256 * 10;
}
