//
//  D64.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 01/08/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#include "D64.hpp"

#include <sys/stat.h>
#include <algorithm>
#include "../PCMTrack.hpp"
#include "../../../Storage/Disk/Encodings/CommodoreGCR.hpp"

using namespace Storage::Disk;

D64::D64(const char *file_name) :
	Storage::FileHolder(file_name)
{
	// in D64, this is it for validation without imposing potential false-negative tests — check that
	// the file size appears to be correct. Stone-age stuff.
	if(file_stats_.st_size != 174848 && file_stats_.st_size != 196608)
		throw ErrorNotD64;

	number_of_tracks_ = (file_stats_.st_size == 174848) ? 35 : 40;

	// then, ostensibly, this is a valid file. Hmmm. Pick a disk ID as a function of the file_name,
	// being the most stable thing available
	disk_id_ = 0;
	while(*file_name)
	{
		disk_id_ ^= file_name[0];
		disk_id_ = (uint16_t)((disk_id_ << 2) ^ (disk_id_ >> 13));
		file_name++;
	}
}

unsigned int D64::get_head_position_count()
{
	return number_of_tracks_*2;
}

std::shared_ptr<Track> D64::get_uncached_track_at_position(unsigned int head, unsigned int position)
{
	// every other track is missing, as is any head above 0
	if(position&1 || head)
		return std::shared_ptr<Track>();

	// figure out where this track starts on the disk
	int offset_to_track = 0;
	int tracks_to_traverse = position >> 1;

	int zone_sizes[] = {17, 7, 6, 10};
	int sectors_by_zone[] = {21, 19, 18, 17};
	int zone = 0;
	for(int current_zone = 0; current_zone < 4; current_zone++)
	{
		int tracks_in_this_zone = std::min(tracks_to_traverse, zone_sizes[current_zone]);
		offset_to_track += tracks_in_this_zone * sectors_by_zone[current_zone];
		tracks_to_traverse -= tracks_in_this_zone;
		if(tracks_in_this_zone == zone_sizes[current_zone]) zone++;
	}

	// seek to start of data
	fseek(file_, offset_to_track * 256, SEEK_SET);

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

	PCMSegment track;
	size_t track_bytes = 349 * (size_t)sectors_by_zone[zone];
	track.number_of_bits = (unsigned int)track_bytes * 8;
	track.data.resize(track_bytes);
	uint8_t *data = &track.data[0];

	memset(data, 0, track_bytes);

	for(int sector = 0; sector < sectors_by_zone[zone]; sector++)
	{
		uint8_t *sector_data = &data[sector * 349];
		sector_data[0] = sector_data[1] = sector_data[2] = 0xff;

		uint8_t sector_number = (uint8_t)(sector);				// sectors count from 0
		uint8_t track_number = (uint8_t)((position >> 1) + 1);	// tracks count from 1
		uint8_t checksum = (uint8_t)(sector_number ^ track_number ^ disk_id_ ^ (disk_id_ >> 8));
		uint8_t header_start[4] = {
			0x08, checksum, sector_number, track_number
		};
		Encodings::CommodoreGCR::encode_block(&sector_data[3], header_start);

		uint8_t header_end[4] = {
			(uint8_t)(disk_id_ & 0xff), (uint8_t)(disk_id_ >> 8), 0, 0
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
		fread(source_data, 1, 256, file_);

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
		while((source_data_offset+4) < 256)
		{
			Encodings::CommodoreGCR::encode_block(&sector_data[target_data_offset], &source_data[source_data_offset]);
			target_data_offset += 5;
			source_data_offset += 4;
		}
		uint8_t end_of_data[4] = {
			source_data[255], checksum, 0, 0
		};
		Encodings::CommodoreGCR::encode_block(&sector_data[target_data_offset], end_of_data);
	}

	return std::shared_ptr<Track>(new PCMTrack(std::move(track)));
}
