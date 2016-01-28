//
//  TapeUEF.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 18/01/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#include "TapeUEF.hpp"
#include <string.h>

uint8_t dr;

Storage::UEF::UEF(const char *file_name) :
	_chunk_id(0), _chunk_length(0), _chunk_position(0),
	_time_base(1200)
{
	_file = gzopen(file_name, "rb");

	char identifier[10];
	int bytes_read = gzread(_file, identifier, 10);
	if(bytes_read < 10 || strcmp(identifier, "UEF File!"))
	{
		throw ErrorNotUEF;
	}

	int minor, major;
	minor = gzgetc(_file);
	major = gzgetc(_file);

	if(major > 0 || minor > 10 || major < 0 || minor < 0)
	{
		throw ErrorNotUEF;
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

	if(!_bit_position && chunk_is_finished())
	{
		find_next_tape_chunk();
	}

	switch(_chunk_id)
	{
		case 0x0100: case 0x0102:
		{
			// In the ordinary ("1200 baud") data encoding format,
			// a zero bit is encoded as one complete cycle at the base frequency.
			// A one bit is two complete cycles at twice the base frequency.

			if(!_bit_position)
			{
				_current_bit = get_next_bit();
			}

			next_pulse.type = (_bit_position&1) ? Pulse::High : Pulse::Low;
			next_pulse.length.length = _current_bit ? 1 : 2;
			next_pulse.length.clock_rate = _time_base * 4;
			_bit_position = (_bit_position+1)&(_current_bit ? 3 : 1);
		} break;

		case 0x0110:
			next_pulse.type = (_bit_position&1) ? Pulse::High : Pulse::Low;
			next_pulse.length.length = 1;
			next_pulse.length.clock_rate = _time_base * 4;
			_bit_position ^= 1;

			if(!_bit_position) _chunk_position++;
		break;

		case 0x0112:
		case 0x0116:
			next_pulse.type = Pulse::Zero;
			next_pulse.length = _chunk_duration;
			_chunk_position++;
		break;
	}

	return next_pulse;
}

void Storage::UEF::find_next_tape_chunk()
{
	int reset_count = 0;
	_chunk_position = 0;
	_bit_position = 0;

	while(1)
	{
		// read chunk ID
		_chunk_id = (uint16_t)gzgetc(_file);
		_chunk_id |= (uint16_t)(gzgetc(_file) << 8);

		_chunk_length = (uint32_t)(gzgetc(_file) << 0);
		_chunk_length |= (uint32_t)(gzgetc(_file) << 8);
		_chunk_length |= (uint32_t)(gzgetc(_file) << 16);
		_chunk_length |= (uint32_t)(gzgetc(_file) << 24);

		if(gzeof(_file))
		{
			reset_count++;
			if(reset_count == 2) break;
			reset();
			continue;
		}

		switch(_chunk_id)
		{
			case 0x0100: case 0x0102: // implicit and explicit bit patterns
			return;

			case 0x0112:
				_chunk_duration.length = (uint16_t)gzgetc(_file);
				_chunk_duration.length |= (uint16_t)(gzgetc(_file) << 8);
				_chunk_duration.clock_rate = _time_base;
			return;

			case 0x0116: // gaps
			return;

			case 0x0110: // carrier tone
				_chunk_duration.length = (uint16_t)gzgetc(_file);
				_chunk_duration.length |= (uint16_t)(gzgetc(_file) << 8);
				gzseek(_file, _chunk_length - 2, SEEK_CUR);
			return;
			case 0x0111: // carrier tone with dummy byte
				// TODO: read lengths
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

bool Storage::UEF::chunk_is_finished()
{
	switch(_chunk_id)
	{
		case 0x0100: return (_chunk_position / 10) == _chunk_length;
		case 0x0102: return (_chunk_position / 8) == _chunk_length;
		case 0x0110: return _chunk_position == _chunk_duration.length;

		case 0x0112:
		case 0x0116: return _chunk_position ? true : false;

		default: return true;
	}
}

bool Storage::UEF::get_next_bit()
{
	switch(_chunk_id)
	{
		case 0x0100:
		{
			uint32_t bit_position = _chunk_position%10;
			_chunk_position++;
			if(!bit_position)
			{
				dr = _current_byte = (uint8_t)gzgetc(_file);
			}
			if(bit_position == 0) return false;
			if(bit_position == 9) return true;
			bool result = (_current_byte&1) ? true : false;
			_current_byte >>= 1;
			return result;
		}
		break;

		case 0x0102:
		{
			uint32_t bit_position = _chunk_position%8;
			_chunk_position++;
			if(!bit_position)
			{
				_current_byte = (uint8_t)gzgetc(_file);
			}
			bool result = (_current_byte&1) ? true : false;
			_current_byte >>= 1;
			return result;
		}
		break;

		case 0x0110:
			_chunk_position++;
		return true;

		default: return true;
	}
}
