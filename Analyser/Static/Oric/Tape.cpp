//
//  Tape.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 06/11/2016.
//  Copyright 2016 Thomas Harte. All rights reserved.
//

#include "Tape.hpp"
#include "../../../Storage/Tape/Parsers/Oric.hpp"

using namespace Analyser::Static::Oric;

std::vector<File> Analyser::Static::Oric::GetFiles(const std::shared_ptr<Storage::Tape::Tape> &tape) {
	std::vector<File> files;
	Storage::Tape::Oric::Parser parser;

	while(!tape->is_at_end()) {
		// sync to next lead-in, check that it's one of three 0x16s
		bool is_fast = parser.sync_and_get_encoding_speed(tape);
		int next_bytes[2];
		next_bytes[0] = parser.get_next_byte(tape, is_fast);
		next_bytes[1] = parser.get_next_byte(tape, is_fast);

		if(next_bytes[0] != 0x16 || next_bytes[1] != 0x16) continue;

		// get the first byte that isn't a 0x16, check it was a 0x24
		int byte = 0x16;
		while(!tape->is_at_end() && byte == 0x16) {
			byte = parser.get_next_byte(tape, is_fast);
		}
		if(byte != 0x24) continue;

		// skip two empty bytes
		parser.get_next_byte(tape, is_fast);
		parser.get_next_byte(tape, is_fast);

		// get data and launch types
		File new_file;
		switch(parser.get_next_byte(tape, is_fast)) {
			case 0x00:	new_file.data_type = File::ProgramType::BASIC;			break;
			case 0x80:	new_file.data_type = File::ProgramType::MachineCode;	break;
			default:	new_file.data_type = File::ProgramType::None;			break;
		}
		switch(parser.get_next_byte(tape, is_fast)) {
			case 0x80:	new_file.launch_type = File::ProgramType::BASIC;		break;
			case 0xc7:	new_file.launch_type = File::ProgramType::MachineCode;	break;
			default:	new_file.launch_type = File::ProgramType::None;			break;
		}

		// read end and start addresses
		new_file.ending_address = uint16_t(parser.get_next_byte(tape, is_fast) << 8);
		new_file.ending_address |= uint16_t(parser.get_next_byte(tape, is_fast));
		new_file.starting_address = uint16_t(parser.get_next_byte(tape, is_fast) << 8);
		new_file.starting_address |= uint16_t(parser.get_next_byte(tape, is_fast));

		// skip an empty byte
		parser.get_next_byte(tape, is_fast);

		// read file name, up to 16 characters and null terminated
		char file_name[17];
		int name_pos = 0;
		while(name_pos < 16) {
			file_name[name_pos] = char(parser.get_next_byte(tape, is_fast));
			if(!file_name[name_pos]) break;
			name_pos++;
		}
		file_name[16] = '\0';
		new_file.name = file_name;

		// read body
		std::size_t body_length = new_file.ending_address - new_file.starting_address + 1;
		new_file.data.reserve(body_length);
		for(std::size_t c = 0; c < body_length; c++) {
			new_file.data.push_back(uint8_t(parser.get_next_byte(tape, is_fast)));
		}

		// only one validation check: was there enough tape?
		if(!tape->is_at_end()) {
			files.push_back(new_file);
		}
	}

	return files;
}
