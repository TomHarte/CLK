//
//  CommodoreTAP.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 25/06/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#include "CommodoreTAP.hpp"
#include <cstdio>
#include <cstring>

using namespace Storage::Tape;

CommodoreTAP::CommodoreTAP(const char *file_name) : _is_at_end(false)
{
	_file = fopen(file_name, "rb");

	if(!_file)
		throw ErrorNotCommodoreTAP;

	// read and check the file signature
	char signature[12];
	if(fread(signature, 1, 12, _file) != 12)
		throw ErrorNotCommodoreTAP;

	if(memcmp(signature, "C64-TAPE-RAW", 12))
		throw ErrorNotCommodoreTAP;

	// check the file version
	int version = fgetc(_file);
	switch(version)
	{
		case 0:		_updated_layout = false;	break;
		case 1:		_updated_layout = true;		break;
		default:	throw ErrorNotCommodoreTAP;
	}

	// skip reserved bytes
	fseek(_file, 3, SEEK_CUR);

	// read file size
	_file_size = (uint32_t)fgetc(_file);
	_file_size |= (uint32_t)(fgetc(_file) << 8);
	_file_size |= (uint32_t)(fgetc(_file) << 16);
	_file_size |= (uint32_t)(fgetc(_file) << 24);

	// set up for pulse output at the PAL clock rate, with each high and
	// low being half of whatever length values will be read; pretend that
	// a high pulse has just been distributed to imply that the next thing
	// that needs to happen is a length check
	_current_pulse.length.clock_rate = 985248 * 2;
	_current_pulse.type = Pulse::High;
}

CommodoreTAP::~CommodoreTAP()
{
	fclose(_file);
}

void CommodoreTAP::virtual_reset()
{
	fseek(_file, 0x14, SEEK_SET);
	_current_pulse.type = Pulse::High;
	_is_at_end = false;
}

bool CommodoreTAP::is_at_end()
{
	return _is_at_end;
}

Storage::Tape::Tape::Pulse CommodoreTAP::virtual_get_next_pulse()
{
	if(_is_at_end)
	{
		return _current_pulse;
	}

	if(_current_pulse.type == Pulse::High)
	{
		uint32_t next_length;
		uint8_t next_byte = (uint8_t)fgetc(_file);
		if(!_updated_layout || next_byte > 0)
		{
			next_length = (uint32_t)next_byte << 3;
		}
		else
		{
			next_length = (uint32_t)fgetc(_file);
			next_length |= (uint32_t)(fgetc(_file) << 8);
			next_length |= (uint32_t)(fgetc(_file) << 16);
		}

		if(feof(_file))
		{
			_is_at_end = true;
			_current_pulse.length.length = _current_pulse.length.clock_rate;
			_current_pulse.type = Pulse::Zero;
		}
		else
		{
			_current_pulse.length.length = next_length;
			_current_pulse.type = Pulse::Low;
		}
	}
	else
		_current_pulse.type = Pulse::High;

	return _current_pulse;
}
