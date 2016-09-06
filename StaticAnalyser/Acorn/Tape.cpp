//
//  Tape.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 29/08/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#include "Tape.hpp"

#include <deque>
#include "../TapeParser.hpp"

using namespace StaticAnalyser::Acorn;

enum class WaveType {
	Short, Long, Unrecognised
};

enum class SymbolType {
	One, Zero
};

class Acorn1200BaudTapeParser: public StaticAnalyer::TapeParser<WaveType, SymbolType> {
	public:
		Acorn1200BaudTapeParser(const std::shared_ptr<Storage::Tape::Tape> &tape) : TapeParser(tape) {}

		int get_next_bit()
		{
			SymbolType symbol = get_next_symbol();
			return (symbol == SymbolType::One) ? 1 : 0;
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

		void reset_crc()	{	_crc = 0;		}
		uint16_t get_crc()	{	return _crc;	}

	private:
		void process_pulse(Storage::Tape::Tape::Pulse pulse)
		{
			switch(pulse.type)
			{
				default: break;
				case Storage::Tape::Tape::Pulse::High:
				case Storage::Tape::Tape::Pulse::Low:
					float pulse_length = pulse.length.get_float();
					if(pulse_length >= 0.35 / 2400.0 && pulse_length < 0.7 / 2400.0) { push_wave(WaveType::Short); return; }
					if(pulse_length >= 0.35 / 1200.0 && pulse_length < 0.7 / 1200.0) { push_wave(WaveType::Long); return; }
				break;
			}

			push_wave(WaveType::Unrecognised);
		}

		void inspect_waves(const std::vector<WaveType> &waves)
		{
			while(waves.size() && waves[0] == WaveType::Unrecognised)
			{
				remove_waves(1);
				return;
			}

			if(waves.size() >= 2 && waves[0] == WaveType::Long && waves[1] == WaveType::Long)
			{
				push_symbol(SymbolType::Zero, 2);
				return;
			}

			if(waves.size() >= 4)
			{
				// If this makes a 1, post it.
				if(	waves[0] == WaveType::Short &&
					waves[1] == WaveType::Short &&
					waves[2] == WaveType::Short &&
					waves[3] == WaveType::Short)
				{
					push_symbol(SymbolType::One, 4);
					return;
				}

				// Otherwise, eject at least one wave as all options are exhausted.
				remove_waves(1);
				return;
			}
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

		uint16_t _crc;
};

static std::unique_ptr<File::Chunk> GetNextChunk(Acorn1200BaudTapeParser &parser)
{
	std::unique_ptr<File::Chunk> new_chunk(new File::Chunk);
	int shift_register = 0;

// TODO: move this into the parser
#define shift()	shift_register = (shift_register >> 1) |  (parser.get_next_bit() << 9)

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

#undef shift

	parser.reset_crc();
	parser.reset_error_flag();

	// read out name
	char name[11];
	int name_ptr = 0;
	while(!parser.is_at_end() && name_ptr < sizeof(name))
	{
		name[name_ptr] = (char)parser.get_next_byte();
		if(!name[name_ptr]) break;
		name_ptr++;
	}
	name[sizeof(name)-1] = '\0';
	new_chunk->name = name;

	// addresses
	new_chunk->load_address = (uint32_t)parser.get_next_word();
	new_chunk->execution_address = (uint32_t)parser.get_next_word();
	new_chunk->block_number = (uint16_t)parser.get_next_short();
	new_chunk->block_length = (uint16_t)parser.get_next_short();
	new_chunk->block_flag = (uint8_t)parser.get_next_byte();
	new_chunk->next_address = (uint32_t)parser.get_next_word();

	uint16_t calculated_header_crc = parser.get_crc();
	uint16_t stored_header_crc = (uint16_t)parser.get_next_short();
	stored_header_crc = (uint16_t)((stored_header_crc >> 8) | (stored_header_crc << 8));
	new_chunk->header_crc_matched = stored_header_crc == calculated_header_crc;

	parser.reset_crc();
	new_chunk->data.reserve(new_chunk->block_length);
	for(int c = 0; c < new_chunk->block_length; c++)
	{
		new_chunk->data.push_back((uint8_t)parser.get_next_byte());
	}

	if(new_chunk->block_length && !(new_chunk->block_flag&0x40))
	{
		uint16_t calculated_data_crc = parser.get_crc();
		uint16_t stored_data_crc = (uint16_t)parser.get_next_short();
		stored_data_crc = (uint16_t)((stored_data_crc >> 8) | (stored_data_crc << 8));
		new_chunk->data_crc_matched = stored_data_crc == calculated_data_crc;
	}
	else
	{
		new_chunk->data_crc_matched = true;
	}

	return parser.get_error_flag() ? nullptr : std::move(new_chunk);
}

std::unique_ptr<File> GetNextFile(std::deque<File::Chunk> &chunks)
{
	// find next chunk with a block number of 0
	while(chunks.size() && chunks.front().block_number)
	{
		chunks.pop_front();
	}

	if(!chunks.size()) return nullptr;

	// accumulate chunks for as long as block number is sequential and the end-of-file bit isn't set
	std::unique_ptr<File> file(new File);

	uint16_t block_number = 0;

	while(chunks.size())
	{
		if(chunks.front().block_number != block_number) return nullptr;

		bool was_last = chunks.front().block_flag & 0x80;
		file->chunks.push_back(chunks.front());
		chunks.pop_front();
		block_number++;

		if(was_last) break;
	}

	// accumulate total data, copy flags appropriately
	file->name = file->chunks.front().name;
	file->load_address = file->chunks.front().load_address;
	file->execution_address = file->chunks.front().execution_address;
	file->is_protected = !!(file->chunks.back().block_flag & 0x01);	// I think the last flags are the ones that count; TODO: check.

	// copy all data into a single big block
	for(File::Chunk chunk : file->chunks)
	{
		file->data.insert(file->data.end(), chunk.data.begin(), chunk.data.end());
	}

	return file;
}

std::list<File> StaticAnalyser::Acorn::GetFiles(const std::shared_ptr<Storage::Tape::Tape> &tape)
{
	Acorn1200BaudTapeParser parser(tape);

	// populate chunk list
	std::deque<File::Chunk> chunk_list;
	while(!parser.is_at_end())
	{
		std::unique_ptr<File::Chunk> chunk = GetNextChunk(parser);
		if(chunk)
		{
			chunk_list.push_back(*chunk);
		}
	}

	// decompose into file list
	std::list<File> file_list;

	while(chunk_list.size())
	{
		std::unique_ptr<File> next_file = GetNextFile(chunk_list);
		if(next_file)
		{
			file_list.push_back(*next_file);
		}
	}

	return file_list;
}
