//
//  Tape.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 24/08/2016.
//  Copyright 2016 Thomas Harte. All rights reserved.
//

#include "Tape.hpp"
#include "Storage/Tape/Parsers/Commodore.hpp"

#include <algorithm>

using namespace Analyser::Static::Commodore;

std::vector<File> Analyser::Static::Commodore::GetFiles(Storage::Tape::TapeSerialiser &serialiser, TargetPlatform::Type type) {
	Storage::Tape::Commodore::Parser parser(type);
	std::vector<File> file_list;

	std::unique_ptr<Storage::Tape::Commodore::Header> header = parser.get_next_header(serialiser);

	while(!serialiser.is_at_end()) {
		if(!header) {
			header = parser.get_next_header(serialiser);
			continue;
		}

		switch(header->type) {
			case Storage::Tape::Commodore::Header::DataSequenceHeader: {
				File &new_file = file_list.emplace_back();
				new_file.name = header->name;
				new_file.raw_name = header->raw_name;
				new_file.starting_address = header->starting_address;
				new_file.ending_address = header->ending_address;
				new_file.type = File::DataSequence;

				new_file.data.swap(header->data);
				while(!serialiser.is_at_end()) {
					header = parser.get_next_header(serialiser);
					if(!header) continue;
					if(header->type != Storage::Tape::Commodore::Header::DataBlock) break;
					std::ranges::copy(header->data, std::back_inserter(new_file.data));
				}
			}
			break;

			case Storage::Tape::Commodore::Header::RelocatableProgram:
			case Storage::Tape::Commodore::Header::NonRelocatableProgram: {
				std::unique_ptr<Storage::Tape::Commodore::Data> data = parser.get_next_data(serialiser);
				if(data) {
					File &new_file = file_list.emplace_back();
					new_file.name = header->name;
					new_file.raw_name = header->raw_name;
					new_file.starting_address = header->starting_address;
					new_file.ending_address = header->ending_address;
					new_file.data.swap(data->data);
					new_file.type =
						header->type == Storage::Tape::Commodore::Header::RelocatableProgram
							? File::RelocatableProgram : File::NonRelocatableProgram;
				}

				header = parser.get_next_header(serialiser);
			}
			break;

			default:
				header = parser.get_next_header(serialiser);
			break;
		}
	}

	return file_list;
}
