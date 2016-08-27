//
//  TapeUEF.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 18/01/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#include "TapeUEF.hpp"
#include <string.h>
#include <math.h>

using namespace Storage;

static float gzgetfloat(gzFile file)
{
	uint8_t bytes[4];
	bytes[0] = (uint8_t)gzgetc(file);
	bytes[1] = (uint8_t)gzgetc(file);
	bytes[2] = (uint8_t)gzgetc(file);
	bytes[3] = (uint8_t)gzgetc(file);

	/* assume a four byte array named Float exists, where Float[0]
	was the first byte read from the UEF, Float[1] the second, etc */

	/* decode mantissa */
	int mantissa;
	mantissa = bytes[0] | (bytes[1] << 8) | ((bytes[2]&0x7f)|0x80) << 16;

	float result = (float)mantissa;
	result = (float)ldexp(result, -23);

	/* decode exponent */
	int exponent;
	exponent = ((bytes[2]&0x80) >> 7) | (bytes[3]&0x7f) << 1;
	exponent -= 127;
	result = (float)ldexp(result, exponent);

	/* flip sign if necessary */
	if(bytes[3]&0x80)
		result = -result;

	return result;
}

TapeUEF::TapeUEF(const char *file_name) :
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

	_start_of_next_chunk = gztell(_file);
	find_next_tape_chunk();
}

TapeUEF::~TapeUEF()
{
	gzclose(_file);
}

void TapeUEF::reset()
{
	gzseek(_file, 12, SEEK_SET);
}

Tape::Pulse TapeUEF::get_next_pulse()
{
	Pulse next_pulse;

	if(!_bit_position && chunk_is_finished())
	{
		find_next_tape_chunk();
	}

	switch(_chunk_id)
	{
		case 0x0100: case 0x0102:
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
		break;

		case 0x0110:
			next_pulse.type = (_bit_position&1) ? Pulse::High : Pulse::Low;
			next_pulse.length.length = 1;
			next_pulse.length.clock_rate = _time_base * 4;
			_bit_position ^= 1;

			if(!_bit_position) _chunk_position++;
		break;

		case 0x0114:
			if(!_bit_position)
			{
				_current_bit = get_next_bit();
				if(_first_is_pulse && !_chunk_position)
				{
					_bit_position++;
				}
			}

			next_pulse.type = (_bit_position&1) ? Pulse::High : Pulse::Low;
			next_pulse.length.length = _current_bit ? 1 : 2;
			next_pulse.length.clock_rate = _time_base * 4;
			_bit_position ^= 1;

			if((_chunk_id == 0x0114) && (_chunk_position == _chunk_duration.length-1) && _last_is_pulse)
			{
				_chunk_position++;
			}
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

void TapeUEF::find_next_tape_chunk()
{
	int reset_count = 0;
	_chunk_position = 0;
	_bit_position = 0;

	while(1)
	{
		gzseek(_file, _start_of_next_chunk, SEEK_SET);

		// read chunk ID
		_chunk_id = (uint16_t)gzgetc(_file);
		_chunk_id |= (uint16_t)(gzgetc(_file) << 8);

		_chunk_length = (uint32_t)(gzgetc(_file) << 0);
		_chunk_length |= (uint32_t)(gzgetc(_file) << 8);
		_chunk_length |= (uint32_t)(gzgetc(_file) << 16);
		_chunk_length |= (uint32_t)(gzgetc(_file) << 24);

		_start_of_next_chunk = gztell(_file) + _chunk_length;

		if(gzeof(_file))
		{
			reset_count++;
			if(reset_count == 2) break;
			reset();
			continue;
		}

		switch(_chunk_id)
		{
			case 0x0100: // implicit bit pattern
				_implicit_data_chunk.position = 0;
			return;

			case 0x0102: // explicit bit patterns
				_explicit_data_chunk.position = 0;
			return;

			case 0x0112: // integer gap
				_chunk_duration.length = (uint16_t)gzgetc(_file);
				_chunk_duration.length |= (uint16_t)(gzgetc(_file) << 8);
				_chunk_duration.clock_rate = _time_base;
			return;

			case 0x0116: // floating point gap
			{
				float length = gzgetfloat(_file);
				_chunk_duration.length = (unsigned int)(length * 4000000);
				_chunk_duration.clock_rate = 4000000;
			}
			return;

			case 0x0110: // carrier tone
				_chunk_duration.length = (uint16_t)gzgetc(_file);
				_chunk_duration.length |= (uint16_t)(gzgetc(_file) << 8);
				gzseek(_file, _chunk_length - 2, SEEK_CUR);
			return;
//			case 0x0111: // carrier tone with dummy byte
				// TODO: read lengths
//			return;
			case 0x0114: // security cycles
			{
				// read number of cycles
				_chunk_duration.length = (uint32_t)gzgetc(_file);
				_chunk_duration.length |= (uint32_t)gzgetc(_file) << 8;
				_chunk_duration.length |= (uint32_t)gzgetc(_file) << 16;

				// Ps and Ws
				_first_is_pulse = gzgetc(_file) == 'P';
				_last_is_pulse = gzgetc(_file) == 'P';
			}
			break;

			case 0x113: // change of base rate
			{
				// TODO: something smarter than just converting this to an int
				float new_time_base = gzgetfloat(_file);
				_time_base = (unsigned int)roundf(new_time_base);
			}
			break;

			default:
				gzseek(_file, _chunk_length, SEEK_CUR);
			break;
		}
	}
}

bool TapeUEF::chunk_is_finished()
{
	switch(_chunk_id)
	{
		case 0x0100: return (_implicit_data_chunk.position / 10) == _chunk_length;
		case 0x0102: return (_explicit_data_chunk.position / 8) == _chunk_length;
		case 0x0114:
		case 0x0110: return _chunk_position == _chunk_duration.length;

		case 0x0112:
		case 0x0116: return _chunk_position ? true : false;

		default: return true;
	}
}

bool TapeUEF::get_next_bit()
{
	switch(_chunk_id)
	{
		case 0x0100:
		{
			uint32_t bit_position = _implicit_data_chunk.position%10;
			_implicit_data_chunk.position++;
			if(!bit_position) _implicit_data_chunk.current_byte = (uint8_t)gzgetc(_file);
			if(bit_position == 0) return false;
			if(bit_position == 9) return true;
			bool result = (_implicit_data_chunk.current_byte&1) ? true : false;
			_implicit_data_chunk.current_byte >>= 1;
			return result;
		}
		break;

		case 0x0102:
		{
			uint32_t bit_position = _explicit_data_chunk.position%8;
			_explicit_data_chunk.position++;
			if(!bit_position) _explicit_data_chunk.current_byte = (uint8_t)gzgetc(_file);
			bool result = (_explicit_data_chunk.current_byte&1) ? true : false;
			_explicit_data_chunk.current_byte >>= 1;
			return result;
		}
		break;

		// TODO: 0x0104, 0x0111

		case 0x0114:
		{
			uint32_t bit_position = _chunk_position%8;
			_chunk_position++;
			if(!bit_position && _chunk_position < _chunk_duration.length)
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
