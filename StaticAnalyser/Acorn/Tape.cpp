//
//  Tape.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 29/08/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#include "Tape.hpp"

#include <deque>

using namespace StaticAnalyser::Acorn;

/*!
	A partly-abstract base class to help in the authorship of tape format parsers;
	provides hooks for a 
*/
template <typename WaveType, typename SymbolType> class TapeParser {
	public:
		TapeParser(const std::shared_ptr<Storage::Tape::Tape> &tape) : _tape(tape) {}

		std::unique_ptr<SymbolType> get_next_symbol()
		{
			while(!_tape->is_at_end())
			{
				std::unique_ptr<SymbolType> symbol = dequeue_next_symbol(_wave_queue);
				if(symbol) return symbol;

				while(!_tape->is_at_end())
				{
					Storage::Tape::Tape::Pulse next_pulse = _tape->get_next_pulse();
					std::unique_ptr<WaveType> next_wave = get_wave_type_for_pulse(next_pulse);
					if(next_wave)
					{
						_wave_queue.push_back(*next_wave);
						break;
					}
				}
			}

			return nullptr;
		}

		void reset_error_flag()		{	_error_flag = false;		}
		bool get_error_flag()		{	return _error_flag;			}
		bool is_at_end()			{	return _tape->is_at_end();	}

	protected:
		bool _error_flag;

	private:
		virtual std::unique_ptr<WaveType> get_wave_type_for_pulse(Storage::Tape::Tape::Pulse) = 0;
		virtual std::unique_ptr<SymbolType> dequeue_next_symbol(std::deque<WaveType> &_wave_queue) = 0;
		std::deque<WaveType> _wave_queue;
		std::shared_ptr<Storage::Tape::Tape> _tape;
};

enum class WaveType {
	Short, Long, Unrecognised
};

enum class SymbolType {
	One, Zero
};

class Acorn1200BaudTapeParser: public TapeParser<WaveType, SymbolType> {
	public:
		Acorn1200BaudTapeParser(const std::shared_ptr<Storage::Tape::Tape> &tape) : TapeParser(tape) {}

		int get_next_bit()
		{
			std::unique_ptr<SymbolType> symbol = get_next_symbol();
			return (symbol && *symbol == SymbolType::One) ? 1 : 0;
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
		std::unique_ptr<WaveType> get_wave_type_for_pulse(Storage::Tape::Tape::Pulse pulse)
		{
			WaveType wave_type = WaveType::Unrecognised;
			switch(pulse.type)
			{
				default: break;
				case Storage::Tape::Tape::Pulse::High:
				case Storage::Tape::Tape::Pulse::Low:
					float pulse_length = pulse.length.get_float();
					if(pulse_length >= 0.35 / 2400.0 && pulse_length < 0.7 / 2400.0) wave_type = WaveType::Short;
					if(pulse_length >= 0.35 / 1200.0 && pulse_length < 0.7 / 1200.0) wave_type = WaveType::Long;
				break;
			}

			return std::unique_ptr<WaveType>(new WaveType(wave_type));
		}

		std::unique_ptr<SymbolType> dequeue_next_symbol(std::deque<WaveType> &_wave_queue)
		{
			while(_wave_queue.size() && _wave_queue.front() == WaveType::Unrecognised)
			{
				_wave_queue.pop_front();
			}

			if(_wave_queue.size() >= 2 && _wave_queue[0] == WaveType::Long && _wave_queue[1] == WaveType::Long)
			{
				_wave_queue.erase(_wave_queue.begin(), _wave_queue.begin()+2);
				return std::unique_ptr<SymbolType>(new SymbolType(SymbolType::Zero));
			}

			if(	_wave_queue.size() >= 4 &&
				_wave_queue[0] == WaveType::Short &&
				_wave_queue[1] == WaveType::Short &&
				_wave_queue[2] == WaveType::Short &&
				_wave_queue[3] == WaveType::Short)
			{
				_wave_queue.erase(_wave_queue.begin(), _wave_queue.begin()+4);
				return std::unique_ptr<SymbolType>(new SymbolType(SymbolType::One));
			}

			return nullptr;
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
