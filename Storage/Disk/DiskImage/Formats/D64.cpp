//
//  D64.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 01/08/2016.
//  Copyright 2016 Thomas Harte. All rights reserved.
//

#include "D64.hpp"

#include <algorithm>
#include <cstring>
#include <sys/stat.h>

#include "../../Track/PCMTrack.hpp"
#include "../../Encodings/CommodoreGCR.hpp"

using namespace Storage::Disk;

D64::D64(const std::string &file_name) :
		file_(file_name) {
	// in D64, this is it for validation without imposing potential false-negative tests: check that
	// the file size appears to be correct. Stone-age stuff.
	if(file_.stats().st_size != 174848 && file_.stats().st_size != 196608)
		throw Error::InvalidFormat;

	number_of_tracks_ = (file_.stats().st_size == 174848) ? 35 : 40;

	// then, ostensibly, this is a valid file. Hmmm. Pick a disk ID as a function of the file_name,
	// being the most stable thing available
	disk_id_ = 0;
	for(const auto &character: file_name) {
		disk_id_ ^= character;
		disk_id_ = uint16_t((disk_id_ << 2) ^ (disk_id_ >> 13));
	}
}

HeadPosition D64::get_maximum_head_position() {
	return HeadPosition(number_of_tracks_);
}

std::shared_ptr<Track> D64::get_track_at_position(Track::Address address) {
	// figure out where this track starts on the disk
	int offset_to_track = 0;
	int tracks_to_traverse = address.position.as_int();

	int zone_sizes[] = {17, 7, 6, 10};
	int sectors_by_zone[] = {21, 19, 18, 17};
	int zone = 0;
	for(int current_zone = 0; current_zone < 4; current_zone++) {
		int tracks_in_this_zone = std::min(tracks_to_traverse, zone_sizes[current_zone]);
		offset_to_track += tracks_in_this_zone * sectors_by_zone[current_zone];
		tracks_to_traverse -= tracks_in_this_zone;
		if(tracks_in_this_zone == zone_sizes[current_zone]) zone++;
	}

	// seek to start of data
	file_.seek(offset_to_track * 256, SEEK_SET);

	// build up a PCM sampling of the GCR version of this track

	// format per sector:
	//
	// syncronisation: three $FFs directly in GCR
	// value $08 to announce a header
	// a checksum made of XORing the following four bytes
	// sector number (1 byte)
	// track number (1 byte)
	// disk ID (2 bytes)
	// five GCR bytes of value $55
	// = [6 bytes -> 7.5 GCR bytes] + ... = 21 GCR bytes
	//
	// syncronisation: three $FFs directly in GCR
	// value $07 to announce data
	// 256 data bytes
	// a checksum: the XOR of the previous 256 bytes
	// two bytes of vaue $00
	// = [260 bytes -> 325 GCR bytes] + 3 GCR bytes = 328 GCR bytes
	//
	// = 349 GCR bytes per sector

	std::size_t track_bytes = 349 * size_t(sectors_by_zone[zone]);
	std::vector<uint8_t> data(track_bytes);

	for(int sector = 0; sector < sectors_by_zone[zone]; sector++) {
		uint8_t *sector_data = &data[size_t(sector) * 349];
		sector_data[0] = sector_data[1] = sector_data[2] = 0xff;

		uint8_t sector_number = uint8_t(sector);						// sectors count from 0
		uint8_t track_number = uint8_t(address.position.as_int() + 1);	// tracks count from 1
		uint8_t checksum = uint8_t(sector_number ^ track_number ^ disk_id_ ^ (disk_id_ >> 8));
		uint8_t header_start[4] = {
			0x08, checksum, sector_number, track_number
		};
		Encodings::CommodoreGCR::encode_block(&sector_data[3], header_start);

		uint8_t header_end[4] = {
			uint8_t(disk_id_ & 0xff), uint8_t(disk_id_ >> 8), 0, 0
		};
		Encodings::CommodoreGCR::encode_block(&sector_data[8], header_end);

		// pad out post-header parts
		uint8_t zeros[4] = {0, 0, 0, 0};
		Encodings::CommodoreGCR::encode_block(&sector_data[13], zeros);
		sector_data[18] = 0x52;
		sector_data[19] = 0x94;
		sector_data[20] = 0xaf;

		// get the actual contents
		uint8_t source_data[256];
		file_.read(source_data, sizeof(source_data));

		// compute the latest checksum
		checksum = 0;
		for(int c = 0; c < 256; c++)
			checksum ^= source_data[c];

		// put in another sync
		sector_data[21] = sector_data[22] = sector_data[23] = 0xff;

		// now start writing in the actual data
		uint8_t start_of_data[4] = {
			0x07, source_data[0], source_data[1], source_data[2]
		};
		Encodings::CommodoreGCR::encode_block(&sector_data[24], start_of_data);
		int source_data_offset = 3;
		int target_data_offset = 29;
		while((source_data_offset+4) < 256) {
			Encodings::CommodoreGCR::encode_block(&sector_data[target_data_offset], &source_data[source_data_offset]);
			target_data_offset += 5;
			source_data_offset += 4;
		}
		uint8_t end_of_data[4] = {
			source_data[255], checksum, 0, 0
		};
		Encodings::CommodoreGCR::encode_block(&sector_data[target_data_offset], end_of_data);
	}

	return std::make_shared<PCMTrack>(PCMSegment(data));
}
