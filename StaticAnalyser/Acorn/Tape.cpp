//
//  Tape.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 29/08/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#include "Tape.hpp"

using namespace StaticAnalyser::Acorn;

struct TapeParser {
	float wave_lengths[4];
	int wave_length_pointer;

	TapeParser() : wave_length_pointer(0) {}

	int GetNextBit(const std::shared_ptr<Storage::Tape::Tape> &tape)
	{
		while(!tape->is_at_end())
		{
			// skip any gaps
			Storage::Tape::Tape::Pulse next_pulse = tape->get_next_pulse();
			while(next_pulse.type == Storage::Tape::Tape::Pulse::Zero)
			{
				next_pulse = tape->get_next_pulse();
			}

			wave_lengths[wave_length_pointer] = next_pulse.length.get_float();
			wave_length_pointer++;

			// if first wave is too short or too long, drop it
			if(wave_lengths[0] < 1.0f / 4800.0f || wave_lengths[0] >= 5.0f / 4800.0f)
			{
				rotate(1);
				continue;
			}

			// if first two waves add up to a correct-length cycle, pop them and this is a 0
			if(wave_length_pointer >= 2)
			{
				float length = wave_lengths[0] + wave_lengths[1];
				if(length >= 3.0f / 4800.0f && length < 5.0f / 4800.0f)
				{
					rotate(2);
					return 0;
				}
			}

			// if all four waves add up to a correct-length cycle, pop them and this is a 1
			if(wave_length_pointer >= 4)
			{
				float length = wave_lengths[0] + wave_lengths[1] + wave_lengths[2] + wave_lengths[3];
				if(length >= 3.0f / 4800.0f && length < 5.0f / 4800.0f)
				{
					rotate(4);
					return 1;
				}
			}
		}

		return 0;
	}

	int GetNextByte(const std::shared_ptr<Storage::Tape::Tape> &tape)
	{
		int value = 0;
		int c = 8;
		if(GetNextBit(tape)) return -1;
		while(c--)
		{
			value = (value >> 1) | (GetNextBit(tape) << 7);
		}
		if(!GetNextBit(tape)) return -1;
		return value;
	}

	private:
		void rotate(int places)
		{
			wave_length_pointer -= places;
			if(places < 4) memmove(wave_lengths, &wave_lengths[places], (size_t)(4 - places) * sizeof(float));
		}
};

std::unique_ptr<File> StaticAnalyser::Acorn::GetNextFile(const std::shared_ptr<Storage::Tape::Tape> &tape)
{
	int shift_register = 0;
	TapeParser parser;

#define shift()	\
	shift_register = (shift_register >> 1) |  (parser.GetNextBit(tape) << 9)

	// find next area of high tone
	while(!tape->is_at_end() && (shift_register != 0x3ff))
	{
		shift();
	}

	// find next 0x2a (swallowing stop bit)
	while(!tape->is_at_end() && (shift_register != 0x254))
	{
		shift();
	}

	// read out name
	char name[10];
	int name_ptr = 0;
	while(!tape->is_at_end() && name_ptr < 10)
	{
		name[name_ptr] = (char)parser.GetNextByte(tape);
		if(!name[name_ptr]) break;
		name_ptr++;
	}

	return nullptr;
}
