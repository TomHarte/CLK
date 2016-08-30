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

	TapeParser(const std::shared_ptr<Storage::Tape::Tape> &tape) : _wave_length_pointer(0), _tape(tape) {}

	int get_next_bit()
	{
		while(!_tape->is_at_end())
		{
			// skip any gaps
			Storage::Tape::Tape::Pulse next_pulse = _tape->get_next_pulse();
			while(next_pulse.type == Storage::Tape::Tape::Pulse::Zero)
			{
				next_pulse = _tape->get_next_pulse();
			}

			_wave_lengths[_wave_length_pointer] = next_pulse.length.get_float();
			_wave_length_pointer++;

			// if first wave is too short or too long, drop it
			if(_wave_lengths[0] < 1.0f / 4800.0f || _wave_lengths[0] >= 5.0f / 4800.0f)
			{
				rotate(1);
			}

			// if first two waves add up to a correct-length cycle, pop them and this is a 0
			if(_wave_length_pointer >= 2)
			{
				float length = _wave_lengths[0] + _wave_lengths[1];
				if(length >= 3.0f / 4800.0f && length < 5.0f / 4800.0f)
				{
					rotate(2);
					return 0;
				}
			}

			// if all four waves add up to a correct-length cycle, pop them and this is a 1
			if(_wave_length_pointer == 4)
			{
				float length = _wave_lengths[0] + _wave_lengths[1] + _wave_lengths[2] + _wave_lengths[3];
				if(length >= 3.0f / 4800.0f && length < 5.0f / 4800.0f)
				{
					rotate(4);
					return 1;
				}
				else
				{
					rotate(1);
				}
			}
		}

		return 0;
	}

	int get_next_byte()
	{
		int value = 0;
		int c = 8;
		if(get_next_bit())
		{
			_error_flag = true;
			return -1;
		}
		while(c--)
		{
			value = (value >> 1) | (get_next_bit() << 7);
		}
		if(!get_next_bit())
		{
			_error_flag = true;
			return -1;
		}
		add_to_crc((uint8_t)value);
		return value;
	}

	int get_next_short()
	{
		int result = get_next_byte();
		result |= get_next_byte() << 8;
		return result;
	}

	int get_next_word()
	{
		int result = get_next_short();
		result |= get_next_short() << 8;
		return result;
	}

	void reset_error_flag()
	{
		_error_flag = false;
	}

	bool get_error_flag()
	{
		return _error_flag;
	}

	void reset_crc()
	{
		_crc = 0;
	}

		void add_to_crc(uint8_t value)
		{
			_crc ^= (uint16_t)value << 8;
			for(int c = 0; c < 8; c++)
			{
				uint16_t exclusive_or = (_crc&0x8000) ? 0x1021 : 0x0000;
				_crc = (uint16_t)(_crc << 1) ^ exclusive_or;
			}
		}

	uint16_t get_crc()
	{
		return _crc;
	}

	bool is_at_end()
	{
		return _tape->is_at_end();
	}

	private:
		void rotate(int places)
		{
			_wave_length_pointer -= places;
			if(places < 4) memmove(_wave_lengths, &_wave_lengths[places], (size_t)(4 - places) * sizeof(float));
		}


		uint16_t _crc;
		bool _error_flag;

		float _wave_lengths[4];
		int _wave_length_pointer;
		std::shared_ptr<Storage::Tape::Tape> _tape;
};

static std::unique_ptr<File::Chunk> GetNextChunk(TapeParser &parser)
{
	std::unique_ptr<File::Chunk> new_chunk(new File::Chunk);
	int shift_register = 0;

#define shift()	\
	shift_register = (shift_register >> 1) |  (parser.get_next_bit() << 9)

	// find next area of high tone
	while(!parser.is_at_end() && (shift_register != 0x3ff))
	{
		shift();
	}

	// find next 0x2a (swallowing stop bit)
	while(!parser.is_at_end() && (shift_register != 0x254))
	{
		shift();
	}

	parser.reset_crc();
	parser.reset_error_flag();

	// read out name
	char name[10];
	int name_ptr = 0;
	while(!parser.is_at_end() && name_ptr < 10)
	{
		name[name_ptr] = (char)parser.get_next_byte();
		if(!name[name_ptr]) break;
		name_ptr++;
	}

	// addresses
	new_chunk->load_address = (uint32_t)parser.get_next_word();
	new_chunk->execution_address = (uint32_t)parser.get_next_word();
	new_chunk->block_number = (uint16_t)parser.get_next_short();
	new_chunk->block_length = (uint16_t)parser.get_next_short();
	new_chunk->block_flag = (uint8_t)parser.get_next_byte();
	new_chunk->next_address = (uint32_t)parser.get_next_word();

	uint16_t calculated_crc = parser.get_crc();
	uint16_t stored_crc = (uint16_t)parser.get_next_short();

	printf("%04x v %04x", calculated_crc, stored_crc);

	return nullptr;
}

std::unique_ptr<File> StaticAnalyser::Acorn::GetNextFile(const std::shared_ptr<Storage::Tape::Tape> &tape)
{
	TapeParser parser(tape);

	GetNextChunk(parser);

	return nullptr;
}
