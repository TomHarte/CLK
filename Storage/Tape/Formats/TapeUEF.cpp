//
//  TapeUEF.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 18/01/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#include "TapeUEF.hpp"
#include <string.h>

Storage::UEF::UEF(const char *file_name) :
	_chunk_id(0), _chunk_length(0), _chunk_position(0),
	_time_base(1200)
{
	_file = gzopen(file_name, "rb");

	char identifier[10];
	int bytes_read = gzread(_file, identifier, 10);
	if(bytes_read < 10 || strcmp(identifier, "UEF File!"))
	{
		// exception?
	}

	int minor, major;
	minor = gzgetc(_file);
	major = gzgetc(_file);

	if(major > 0 || minor > 10 || major < 0 || minor < 0)
	{
		// exception?
	}

	find_next_tape_chunk();
}

Storage::UEF::~UEF()
{
	gzclose(_file);
}

void Storage::UEF::reset()
{
	gzseek(_file, 12, SEEK_SET);
}

Storage::Tape::Pulse Storage::UEF::get_next_pulse()
{
	Pulse next_pulse;

	return next_pulse;
}

void Storage::UEF::find_next_tape_chunk()
{
	int reset_count = 0;

	while(1)
	{
		// read chunk ID
		_chunk_id = (uint16_t)gzgetc(_file);
		_chunk_id |= (uint16_t)(gzgetc(_file) << 8);

		_chunk_length = (uint32_t)(gzgetc(_file) << 0);
		_chunk_length |= (uint32_t)(gzgetc(_file) << 8);
		_chunk_length |= (uint32_t)(gzgetc(_file) << 16);
		_chunk_length |= (uint32_t)(gzgetc(_file) << 24);

		printf("%04x: %d\n", _chunk_id, _chunk_length);

		if (gzeof(_file))
		{
			reset_count++;
			if(reset_count == 2) break;
			reset();
			continue;
		}

		switch(_chunk_id)
		{
			case 0x0100: case 0x0102: // implicit and explicit bit patterns
			case 0x0112: case 0x0116: // gaps
			return;

			case 0x0110: // carrier tone
				// TODO: read length
			return;
			case 0x0111: // carrier tone with dummy byte
				// TODO: read length
			return;
			case 0x0114: // security cycles
				// TODO: read number, Ps and Ws
			break;

			case 0x113: // change of base rate
			break;

			default:
				gzseek(_file, _chunk_length, SEEK_CUR);
			break;
		}
	}
}
