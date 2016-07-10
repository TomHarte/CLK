//
//  G64.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 10/07/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#include "G64.hpp"

using namespace Storage;

G64::G64(const char *file_name)
{
	_file = fopen(file_name, "rb");

	if(!_file)
		throw ErrorNotGCR;

	// read and check the file signature
	char signature[8];
	if(fread(signature, 1, 8, _file) != 8)
		throw ErrorNotGCR;

	if(memcmp(signature, "GCR-1541", 8))
		throw ErrorNotGCR;

	// check the version number
	int version = fgetc(_file);
	if(version != 0)
	{
		throw ErrorUnknownVersion;
	}

	// get the number of tracks and track size
	_number_of_tracks = (uint8_t)fgetc(_file);
	_maximum_track_size = (uint16_t)fgetc(_file);
	_maximum_track_size |= (uint16_t)fgetc(_file) << 8;
}

G64::~G64()
{
	if(_file) fclose(_file);
}

unsigned int G64::get_head_position_count()
{
	// give at least 84 tracks, to yield the normal geometry but,
	// if there are more, shove them in
	return _number_of_tracks > 84 ? _number_of_tracks : 84;
}

std::shared_ptr<Track> G64::get_track_at_position(unsigned int position)
{
	std::shared_ptr<Track> resulting_track;

	// if there's definitely no track here, return the empty track
	// (TODO: should be supplying one with an index hole?)
	if(position >= _number_of_tracks) return resulting_track;

	// seek to this track's entry in the track table
	fseek(_file, (long)((position * 4) + 0xc), SEEK_SET);

	// read the track offset
	uint32_t track_offset;
	track_offset = (uint32_t)fgetc(_file);
	track_offset |= (uint32_t)fgetc(_file) << 8;
	track_offset |= (uint32_t)fgetc(_file) << 16;
	track_offset |= (uint32_t)fgetc(_file) << 24;

	// if the track offset is zero, this track doesn't exist, so...
	if(!track_offset) return resulting_track;

	// seek to the track start
	fseek(_file, (int)track_offset, SEEK_SET);

	// get the real track length
	uint16_t track_length;
	track_length = (uint16_t)fgetc(_file);
	track_length |= (uint16_t)fgetc(_file) << 8;

	// grab the byte contents of this track
	uint8_t track_contents[track_length];
	fread(track_contents, 1, track_length, _file);

	// seek to this track's entry in the speed zone table
	fseek(_file, (long)((position * 4) + 0x15c), SEEK_SET);

	// read the speed zone offsrt
	uint32_t speed_zone_offset;
	speed_zone_offset = (uint32_t)fgetc(_file);
	speed_zone_offset |= (uint32_t)fgetc(_file) << 8;
	speed_zone_offset |= (uint32_t)fgetc(_file) << 16;
	speed_zone_offset |= (uint32_t)fgetc(_file) << 24;

	// if the speed zone is not constant, create a track based on the whole table; otherwise create one that's constant
	if(speed_zone_offset > 3)
	{
		// seek to start of speed zone
		fseek(_file, (int)speed_zone_offset, SEEK_SET);

		uint16_t speed_zone_length = (track_length + 3) >> 2;

		// read the speed zone bytes
		uint8_t speed_zone_contents[speed_zone_length];
		fread(speed_zone_contents, 1, speed_zone_length, _file);
	}
	else
	{
	}

	return resulting_track;
}
