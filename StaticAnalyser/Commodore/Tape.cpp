//
//  Tape.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 24/08/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#include "Tape.hpp"

#include "../../Storage/Tape/Parsers/Commodore.hpp"

using namespace StaticAnalyser::Commodore;

std::list<File> StaticAnalyser::Commodore::GetFiles(const std::shared_ptr<Storage::Tape::Tape> &tape)
{
	Storage::Tape::Commodore::Parser parser;
	std::list<File> file_list;

	std::unique_ptr<Storage::Tape::Commodore::Header> header = parser.get_next_header(tape);

	while(!tape->is_at_end())
	{
		if(!header)
		{
			header = parser.get_next_header(tape);
			continue;
		}

		switch(header->type)
		{
			case Storage::Tape::Commodore::Header::DataSequenceHeader:
			{
				File new_file;
				new_file.name = header->name;
				new_file.raw_name = header->raw_name;
				new_file.starting_address = header->starting_address;
				new_file.ending_address = header->ending_address;
				new_file.type = File::DataSequence;

				new_file.data.swap(header->data);
				while(!tape->is_at_end())
				{
					header = parser.get_next_header(tape);
					if(!header) continue;
					if(header->type != Storage::Tape::Commodore::Header::DataBlock) break;
					std::copy(header->data.begin(), header->data.end(), std::back_inserter(new_file.data));
				}

				file_list.push_back(new_file);
			}
			break;

			case Storage::Tape::Commodore::Header::RelocatableProgram:
			case Storage::Tape::Commodore::Header::NonRelocatableProgram:
			{
				std::unique_ptr<Storage::Tape::Commodore::Data> data = parser.get_next_data(tape);
				if(data)
				{
					File new_file;
					new_file.name = header->name;
					new_file.raw_name = header->raw_name;
					new_file.starting_address = header->starting_address;
					new_file.ending_address = header->ending_address;
					new_file.data.swap(data->data);
					new_file.type = (header->type == Storage::Tape::Commodore::Header::RelocatableProgram) ? File::RelocatableProgram : File::NonRelocatableProgram;

					file_list.push_back(new_file);
				}

				header = parser.get_next_header(tape);
			}
			break;

			default:
				header = parser.get_next_header(tape);
			break;
		}
	}

	return file_list;
}
