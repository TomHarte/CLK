//
//  TapePRG.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 14/08/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#include "TapePRG.hpp"

#include <sys/stat.h>

using namespace Storage;

TapePRG::TapePRG(const char *file_name) : _file(nullptr), _bitPhase(3), _filePhase(FilePhaseLeadIn), _phaseOffset(0)
{
	struct stat file_stats;
	stat(file_name, &file_stats);

	// There's really no way to validate other than that if this file is larger than 64kb,
	// of if load address + length > 65536 then it's broken.
	if(file_stats.st_size >= 65538 || file_stats.st_size < 3)
		throw ErrorBadFormat;

	_file = fopen(file_name, "rb");
	if(!_file) throw ErrorBadFormat;

	_load_address = (uint16_t)fgetc(_file);
	_load_address |= (uint16_t)fgetc(_file) << 8;

	if (_load_address + file_stats.st_size >= 65536)
		throw ErrorBadFormat;
}

TapePRG::~TapePRG()
{
	if(_file) fclose(_file);
}

Tape::Pulse TapePRG::get_next_pulse()
{
	static const unsigned int leader_zero_length = 179;
	static const unsigned int zero_length = 169;
	static const unsigned int one_length = 247;
	static const unsigned int word_marker_length = 328;

	_bitPhase = (_bitPhase+1)&3;
	if(!_bitPhase) get_next_output_token();

	Tape::Pulse pulse;
	switch(_outputToken)
	{
		case Leader:		pulse.length.length = leader_zero_length;								break;
		case Zero:			pulse.length.length = (_bitPhase&2) ? one_length : zero_length;			break;
		case One:			pulse.length.length = (_bitPhase&2) ? zero_length : one_length;			break;
		case WordMarker:	pulse.length.length = (_bitPhase&2) ? one_length : word_marker_length;	break;
	}
	pulse.length.clock_rate = 1000000;
	pulse.type = (_bitPhase&1) ? Pulse::Low : Pulse::High;
	return pulse;
}

void TapePRG::reset()
{
	_bitPhase = 3;
	fseek(_file, 2, SEEK_SET);
	_filePhase = FilePhaseLeadIn;
	_phaseOffset = 0;
}

void TapePRG::get_next_output_token()
{
	// the lead-in is 20,000 instances of the lead-in pair; every other phase begins with 5000
	// before doing whatever it should be doing
	if(_filePhase == FilePhaseLeadIn || _phaseOffset < 5000)
	{
		_outputToken = Leader;
		_phaseOffset++;
		if(_filePhase == FilePhaseLeadIn && _phaseOffset == 20000)
		{
			_phaseOffset = 0;
			_filePhase = FilePhaseHeader;
		}
		return;
	}

	// determine whether a new byte needs to be queued up
	int block_offset = _phaseOffset - 5000;
	int bit_offset = block_offset % 10;
	int byte_offset = block_offset / 10;

	if(bit_offset == 0)
	{
		// the first nine bytes are countdown; the high bit is set if this is a header
		if(byte_offset < 9)
		{
			_output_byte = (uint8_t)(9 - block_offset) | 0x80;
		}
		else
		{
		}
	}

	switch(bit_offset)
	{
		case 0:
			_outputToken = WordMarker;
		break;
		default:
			_outputToken = (_output_byte & (1 << (bit_offset - 1))) ? One : Zero;
		break;
		case 1:
		{
			uint8_t parity = _outputToken;
			parity ^= (parity >> 4);
			parity ^= (parity >> 2);
			parity ^= (parity >> 1);
			_outputToken = (parity&1) ? One : Zero;
		}
		break;
	}
}
