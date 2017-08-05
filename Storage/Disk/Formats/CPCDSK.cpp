//
//  CPCDSK.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 05/08/2017.
//  Copyright © 2017 Thomas Harte. All rights reserved.
//

#include "CPCDSK.hpp"

#include "../Encodings/MFM.hpp"

using namespace Storage::Disk;

CPCDSK::CPCDSK(const char *file_name) :
	Storage::FileHolder(file_name), is_extended_(false) {
	if(!check_signature("MV - CPC", 8)) {
		is_extended_ = true;
		if(!check_signature("EXTENDED", 8))
			throw ErrorNotCPCDSK;
	}

	// Don't really care about about the creator; skip.
	fseek(file_, 0x30, SEEK_SET);
	head_position_count_ = (unsigned int)fgetc(file_);
	head_count_ = (unsigned int)fgetc(file_);

	if(is_extended_) {
		for(unsigned int c = 0; c < head_position_count_ * head_count_; c++) {
			track_sizes_.push_back((size_t)(fgetc(file_) << 8));
		}
	} else {
		size_of_a_track_ = fgetc16le();
	}
}

unsigned int CPCDSK::get_head_position_count() {
	return head_position_count_;
}

unsigned int CPCDSK::get_head_count() {
	return head_count_;
}

bool CPCDSK::get_is_read_only() {
	// TODO: allow writing.
	return true;
}

std::shared_ptr<Track> CPCDSK::get_uncached_track_at_position(unsigned int head, unsigned int position) {
	// Given that thesea are interleaved images, determine which track, chronologically, is being requested.
	unsigned int chronological_track = (position * head_count_) + head;

	// All DSK images reserve 0x100 bytes for their headers.
	long file_offset = 0x100;
	if(is_extended_) {
		// Tracks are a variable size in the original DSK file format; sum the lengths
		// of all tracks prior to the interesting one to get a file offset.
		unsigned int t = 0;
		while(t < chronological_track && t < track_sizes_.size()) {
			file_offset += track_sizes_[t];
		}
	} else {
		// Tracks are a fixed size in the original DSK file format.
		file_offset += size_of_a_track_ * chronological_track;
	}

	// Find the track, and skip the unused part of track information.
	fseek(file_, file_offset + 16, SEEK_SET);

	// Grab the track information.
	fseek(file_, 5, SEEK_CUR);	// skip track number, side number, sector size — each is given per sector
	int number_of_sectors = fgetc(file_);
	__unused uint8_t gap3_length = (uint8_t)fgetc(file_);
	__unused uint8_t filler_byte = (uint8_t)fgetc(file_);

	// Grab the sector information
	struct SectorInfo {
		uint8_t track;
		uint8_t side;
		uint8_t sector;
		size_t length;
		uint8_t status1;
		uint8_t status2;
	};
	std::vector<SectorInfo> sector_infos;
	while(number_of_sectors--) {
		SectorInfo new_sector;

		new_sector.track = (uint8_t)fgetc(file_);
		new_sector.side = (uint8_t)fgetc(file_);
		new_sector.sector = (uint8_t)fgetc(file_);
		new_sector.length = (size_t)(128 << fgetc(file_));
		if(new_sector.length == 0x2000) new_sector.length = 0x1800;
		new_sector.status1 = (uint8_t)fgetc(file_);
		new_sector.status2 = (uint8_t)fgetc(file_);
		fseek(file_, 2, SEEK_CUR);

		sector_infos.push_back(new_sector);
	}

	// Get the sectors.
	fseek(file_, file_offset + 0x100, SEEK_SET);
	if(is_extended_) {
		// TODO: everything about extended disk images
	} else {
		std::vector<Storage::Encodings::MFM::Sector> sectors;
		for(auto &sector_info : sector_infos) {
			Storage::Encodings::MFM::Sector new_sector;
			new_sector.track = sector_info.track;
			new_sector.side = sector_info.side;
			new_sector.sector = sector_info.sector;
			new_sector.data.resize(sector_info.length);
			fread(new_sector.data.data(), sizeof(uint8_t), sector_info.length, file_);

			// TODO: obey the status bytes, somehow (?)

			sectors.push_back(std::move(new_sector));
		}

		// TODO: supply gay 3 length and filler byte
		if(sectors.size()) return Storage::Encodings::MFM::GetMFMTrackWithSectors(sectors);
	}

	return nullptr;
}
