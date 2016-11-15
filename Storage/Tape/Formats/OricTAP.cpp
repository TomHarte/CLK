//
//  OricTAP.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 10/10/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#include "OricTAP.hpp"

#include <sys/stat.h>

using namespace Storage::Tape;

OricTAP::OricTAP(const char *file_name) : _file(NULL)
{
	struct stat file_stats;
	stat(file_name, &file_stats);
	_file_length = (size_t)file_stats.st_size;

	_file = fopen(file_name, "rb");

	if(!_file)
		throw ErrorNotOricTAP;

	// read and check the file signature
	uint8_t signature[4];
	if(fread(signature, 1, 4, _file) != 4)
		throw ErrorNotOricTAP;

	if(signature[0] != 0x16 || signature[1] != 0x16 || signature[2] != 0x16 || signature[3] != 0x24)
		throw ErrorNotOricTAP;

	// then rewind and start again
	virtual_reset();
}

OricTAP::~OricTAP()
{
	if(_file) fclose(_file);
}

void OricTAP::virtual_reset()
{
	fseek(_file, 0, SEEK_SET);
	_bit_count = 13;
	_phase = _next_phase = LeadIn;
	_phase_counter = 0;
	_pulse_counter = 0;
}

Tape::Pulse OricTAP::virtual_get_next_pulse()
{
	// Each byte byte is written as 13 bits: 0, eight bits of data, parity, three 1s.
	if(_bit_count == 13)
	{
		if(_next_phase != _phase)
		{
			_phase = _next_phase;
			_phase_counter = 0;
		}

		_bit_count = 0;
		uint8_t next_byte = 0;
		switch(_phase)
		{
			case LeadIn:
				next_byte = _phase_counter < 258 ? 0x16 : 0x24;
				_phase_counter++;
				if(_phase_counter == 259)	// 256 artificial bytes plus the three in the file = 259
				{
					while(1)
					{
						if(fgetc(_file) != 0x16) break;
					}
					_next_phase = Header;
				}
			break;

			case Header:
				// Counts are relative to:
				// [0, 1]:		"two bytes unused" (on the Oric 1)
				// 2:			program type
				// 3:			auto indicator
				// [4, 5]:		end address of data
				// [6, 7]:		start address of data
				// 8:			"unused" (on the Oric 1)
				// [9...]:		filename, up to NULL byte
				next_byte = (uint8_t)fgetc(_file);

				if(_phase_counter == 4)	_data_end_address = (uint16_t)(next_byte << 8);
				if(_phase_counter == 5)	_data_end_address |= next_byte;
				if(_phase_counter == 6)	_data_start_address = (uint16_t)(next_byte << 8);
				if(_phase_counter == 7)	_data_start_address |= next_byte;

				if(_phase_counter >= 9 && !next_byte)	// advance after the filename-ending NULL byte
				{
					_next_phase = Gap;
				}
				if(feof(_file))
				{
					_next_phase = End;
				}
				_phase_counter++;
			break;

			case Gap:
				_phase_counter++;
				if(_phase_counter == 8)
				{
					_next_phase = Data;
				}
			break;

			case Data:
				next_byte = (uint8_t)fgetc(_file);
				_phase_counter++;
				if(_phase_counter >= (_data_end_address - _data_start_address)+1)
				{
					if(next_byte == 0x16)
					{
						_next_phase = LeadIn;
					}
					else if(feof(_file))
					{
						_next_phase = End;
					}
				}
			break;

			case End:
			break;
		}

		uint8_t parity = next_byte;
		parity ^= (parity >> 4);
		parity ^= (parity >> 2);
		parity ^= (parity >> 1);
		_current_value = (uint16_t)(((uint16_t)next_byte << 1) | ((parity&1) << 9) | (7 << 10));
	}

	// In slow mode, a 0 is 4 periods of 1200 Hz, a 1 is 8 periods at 2400 Hz.
	// In fast mode, a 1 is a single period of 2400 Hz, a 0 is a 2400 Hz pulse followed by a 1200 Hz pulse.
	// This code models fast mode.
	Tape::Pulse pulse;
	pulse.length.clock_rate = 4800;
	int next_bit;

	switch(_phase)
	{
		case End:
			pulse.type = Pulse::Zero;
			pulse.length.length = 4800;
		return pulse;

		case Gap:
			_bit_count = 13;
			pulse.type = (_phase_counter&1) ? Pulse::Low : Pulse::High;
			pulse.length.length = 100;
		return pulse;

		default:
			next_bit = _current_value & 1;
		break;
	}

	if(next_bit)
	{
		pulse.length.length = 1;
	}
	else
	{
		pulse.length.length = _pulse_counter ? 2 : 1;
	}
	pulse.type = _pulse_counter ? Pulse::High : Pulse::Low;	// TODO

	_pulse_counter ^= 1;
	if(!_pulse_counter)
	{
		_current_value >>= 1;
		_bit_count++;
	}
	return pulse;
}

bool OricTAP::is_at_end()
{
	return _phase == End;
}
