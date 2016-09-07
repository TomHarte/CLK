//
//  Tape.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 24/08/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#include "Tape.hpp"

#include "../TapeParser.hpp"
#include "Utilities.hpp"

using namespace StaticAnalyser::Commodore;

enum class WaveType {
	Short, Medium, Long, Unrecognised
};

enum class SymbolType {
	One, Zero, Word, EndOfBlock, LeadIn
};

struct Header {
	enum {
		RelocatableProgram,
		NonRelocatableProgram,
		DataSequenceHeader,
		DataBlock,
		EndOfTape,
		Unknown
	} type;

	std::vector<uint8_t> data;
	std::wstring name;
	std::vector<uint8_t> raw_name;
	uint16_t starting_address;
	uint16_t ending_address;
	bool parity_was_valid;
	bool duplicate_matched;
};

struct Data {
	std::vector<uint8_t> data;
	bool parity_was_valid;
	bool duplicate_matched;
};

class CommodoreROMTapeParser: public StaticAnalyer::TapeParser<WaveType, SymbolType> {
	public:
		CommodoreROMTapeParser(const std::shared_ptr<Storage::Tape::Tape> &tape) :
			TapeParser(tape),
			_wave_period(0.0f),
			_previous_was_high(false),
			_parity_byte(0) {}

		/*!
			Advances to the next header block on the tape, then consumes, parses, and returns it.
			Returns @c nullptr if any wave-encoding level errors are encountered.
		*/
		std::unique_ptr<Header> get_next_header()
		{
			std::unique_ptr<Header> header(new Header);
			reset_error_flag();

			// find and proceed beyond lead-in tone
			proceed_to_symbol(SymbolType::LeadIn);

			// look for landing zone
			proceed_to_landing_zone(true);
			reset_parity_byte();

			// get header type
			uint8_t header_type = get_next_byte();
			switch(header_type)
			{
				default:	header->type = Header::Unknown;					break;
				case 0x01:	header->type = Header::RelocatableProgram;		break;
				case 0x02:	header->type = Header::DataBlock;				break;
				case 0x03:	header->type = Header::NonRelocatableProgram;	break;
				case 0x04:	header->type = Header::DataSequenceHeader;		break;
				case 0x05:	header->type = Header::EndOfTape;				break;
			}

			// grab rest of data
			header->data.reserve(191);
			for(size_t c = 0; c < 191; c++)
			{
				header->data.push_back(get_next_byte());
			}

			uint8_t parity_byte = get_parity_byte();
			header->parity_was_valid = get_next_byte() == parity_byte;

			// check that the duplicate matches
			proceed_to_landing_zone(false);
			header->duplicate_matched = true;
			if(get_next_byte() != header_type) header->duplicate_matched = false;
			for(size_t c = 0; c < 191; c++)
			{
				if(header->data[c] != get_next_byte()) header->duplicate_matched = false;
			}
			if(get_next_byte() != parity_byte) header->duplicate_matched = false;

			// parse if this is not pure data
			if(header->type != Header::DataBlock)
			{
				header->starting_address	= (uint16_t)(header->data[0] | (header->data[1] << 8));
				header->ending_address		= (uint16_t)(header->data[2] | (header->data[3] << 8));

				for(size_t c = 0; c < 16; c++)
				{
					header->raw_name.push_back(header->data[4 + c]);
				}
				header->name = petscii_from_bytes(&header->raw_name[0], 16, false);
			}

			if(get_error_flag()) return nullptr;
			return header;
		}

		std::unique_ptr<Data> get_next_data()
		{
			std::unique_ptr<Data> data(new Data);

			// find and proceed beyond lead-in tone to the next landing zone
			proceed_to_symbol(SymbolType::LeadIn);
			proceed_to_landing_zone(true);
			reset_parity_byte();

			// accumulate until the next non-word marker is hit
			while(!is_at_end())
			{
				SymbolType start_symbol = get_next_symbol();
				if(start_symbol != SymbolType::Word) break;
				data->data.push_back(get_next_byte_contents());
			}

			// the above has reead the parity byte to the end of the data; if it matched the calculated parity it'll now be zero
			data->parity_was_valid = !get_parity_byte();

			// compare to the duplicate
			proceed_to_symbol(SymbolType::LeadIn);
			proceed_to_landing_zone(false);
			reset_parity_byte();
			data->duplicate_matched = true;
			for(size_t c = 0; c < data->data.size(); c++)
			{
				if(get_next_byte() != data->data[c]) data->duplicate_matched = false;
			}

			// remove the captured parity
			data->data.erase(data->data.end()-1);

			if(get_error_flag()) return nullptr;
			return data;
		}

		void spin()
		{
			while(!is_at_end())
			{
				SymbolType symbol = get_next_symbol();
				switch(symbol)
				{
					case SymbolType::One:			printf("1");	break;
					case SymbolType::Zero:			printf("0");	break;
					case SymbolType::Word:			printf(" ");	break;
					case SymbolType::EndOfBlock:	printf("\n");	break;
					case SymbolType::LeadIn:		printf("-");	break;
				}
			}
		}

	private:
		/*!
			Finds and completes the next landing zone.
		*/
		void proceed_to_landing_zone(bool is_original)
		{
			uint8_t landing_zone[9] = {0, 0, 0, 0, 0, 0, 0, 0, 0};
			while(!is_at_end())
			{
				memmove(landing_zone, &landing_zone[1], sizeof(uint8_t) * 8);
				landing_zone[8] = get_next_byte();

				bool is_landing_zone = true;
				for(int c = 0; c < 9; c++)
				{
					if(landing_zone[c] != ((is_original ? 0x80 : 0x00) | 0x9) - c)
					{
						is_landing_zone = false;
						break;
					}
				}
				if(is_landing_zone) break;
			}
		}

		/*!
			Swallows symbols until it reaches the first instance of the required symbol, swallows that
			and returns.
		*/
		void proceed_to_symbol(SymbolType required_symbol)
		{
			while(!is_at_end())
			{
				SymbolType symbol = get_next_symbol();
				if(symbol == required_symbol) return;
			}
		}

		/*!
			Swallows the next byte; sets the error flag if it is not equal to @c value.
		*/
		void expect_byte(uint8_t value)
		{
			uint8_t next_byte = get_next_byte();
			if(next_byte != value) _error_flag = true;
		}

		uint8_t _parity_byte;
		void reset_parity_byte()			{ _parity_byte = 0;		}
		uint8_t get_parity_byte()			{ return _parity_byte;	}
		void add_parity_byte(uint8_t byte)	{ _parity_byte ^= byte;	}

		/*!
			Proceeds to the next word marker then returns the result of @c get_next_byte_contents.
		*/
		uint8_t get_next_byte()
		{
			proceed_to_symbol(SymbolType::Word);
			return get_next_byte_contents();
		}

		/*!
			Reads the next nine symbols and applies a binary test to each to differentiate between ::One and not-::One. 
			Returns a byte composed of the first eight of those as bits; sets the error flag if any symbol is not
			::One and not ::Zero, or if the ninth bit is not equal to the odd parity of the other eight.
		*/
		uint8_t get_next_byte_contents()
		{
			int byte_plus_parity = 0;
			int c = 9;
			while(c--)
			{
				SymbolType next_symbol = get_next_symbol();
				if((next_symbol != SymbolType::One) && (next_symbol != SymbolType::Zero)) _error_flag = true;
				byte_plus_parity = (byte_plus_parity >> 1) | (((next_symbol == SymbolType::One) ? 1 : 0) << 8);
			}

			int check = byte_plus_parity;
			check ^= (check >> 4);
			check ^= (check >> 2);
			check ^= (check >> 1);
			if((check&1) == (byte_plus_parity >> 8)) _error_flag = true;

			add_parity_byte((uint8_t)byte_plus_parity);
			return (uint8_t)byte_plus_parity;
		}

		/*!
			Returns the result of two consecutive @c get_next_byte calls, arranged in little-endian format.
		*/
		uint16_t get_next_short()
		{
			uint16_t value = get_next_byte();
			value |= get_next_byte() << 8;
			return value;
		}

		/*!
			Per the contract with StaticAnalyser::TapeParser; sums time across pulses. If this pulse
			indicates a high to low transition, inspects the time since the last transition, to produce
			a long, medium, short or unrecognised wave period.
		*/
		void process_pulse(Storage::Tape::Tape::Pulse pulse)
		{
			bool is_high = pulse.type == Storage::Tape::Tape::Pulse::High;
			if(!is_high && _previous_was_high)
			{
				if(_wave_period >= 0.000592 && _wave_period < 0.000752)			push_wave(WaveType::Long);
				else if(_wave_period >= 0.000432 && _wave_period < 0.000592)	push_wave(WaveType::Medium);
				else if(_wave_period >= 0.000272 && _wave_period < 0.000432)	push_wave(WaveType::Short);
				else push_wave(WaveType::Unrecognised);

				_wave_period = 0.0f;
			}

			_wave_period += pulse.length.get_float();
			_previous_was_high = is_high;
		}
		bool _previous_was_high;
		float _wave_period;

		/*!
			Per the contract with StaticAnalyser::TapeParser; produces any of a word marker, an end-of-block marker, 
			a zero, a one or a lead-in symbol based on the currently captured waves.
		*/
		void inspect_waves(const std::vector<WaveType> &waves)
		{
			if(waves.size() < 2) return;

			if(waves[0] == WaveType::Long && waves[1] == WaveType::Medium)
			{
				push_symbol(SymbolType::Word, 2);
				return;
			}

			if(waves[0] == WaveType::Long && waves[1] == WaveType::Short)
			{
				push_symbol(SymbolType::EndOfBlock, 2);
				return;
			}

			if(waves[0] == WaveType::Short && waves[1] == WaveType::Medium)
			{
				push_symbol(SymbolType::Zero, 2);
				return;
			}

			if(waves[0] == WaveType::Medium && waves[1] == WaveType::Short)
			{
				push_symbol(SymbolType::One, 2);
				return;
			}

			if(waves[0] == WaveType::Short)
			{
				push_symbol(SymbolType::LeadIn, 1);
				return;
			}

			// Otherwise, eject at least one wave as all options are exhausted.
			remove_waves(1);
		}
};

std::list<File> StaticAnalyser::Commodore::GetFiles(const std::shared_ptr<Storage::Tape::Tape> &tape)
{
	CommodoreROMTapeParser parser(tape);
	std::list<File> file_list;

	std::unique_ptr<Header> header = parser.get_next_header();

	while(!parser.is_at_end())
	{
		if(!header)
		{
			header = parser.get_next_header();
			continue;
		}

		switch(header->type)
		{
			case Header::DataSequenceHeader:
			{
				File new_file;
				new_file.name = header->name;
				new_file.raw_name = header->raw_name;
				new_file.starting_address = header->starting_address;
				new_file.ending_address = header->ending_address;
				new_file.type = File::DataSequence;

				new_file.data.swap(header->data);
				while(1)
				{
					header = parser.get_next_header();
					if(header->type != Header::DataBlock) break;
					std::copy(header->data.begin(), header->data.end(), std::back_inserter(new_file.data));
				}

				file_list.push_back(new_file);
			}
			break;

			case Header::RelocatableProgram:
			case Header::NonRelocatableProgram:
			{
				std::unique_ptr<Data> data = parser.get_next_data();

				File new_file;
				new_file.name = header->name;
				new_file.raw_name = header->raw_name;
				new_file.starting_address = header->starting_address;
				new_file.ending_address = header->ending_address;
				new_file.data.swap(data->data);
				new_file.type = (header->type == Header::RelocatableProgram) ? File::RelocatableProgram : File::NonRelocatableProgram;

				file_list.push_back(new_file);

				header = parser.get_next_header();
			}
			break;

			default:
				header = parser.get_next_header();
			break;
		}
	}


	return file_list;
}
