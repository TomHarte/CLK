//
//  Tape.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 25/12/2017.
//  Copyright 2017 Thomas Harte. All rights reserved.
//

#include "Tape.hpp"

#include "../../../Storage/Tape/Parsers/MSX.hpp"

using namespace Analyser::Static::MSX;

File::File(File &&rhs) :
	name(std::move(rhs.name)),
	type(rhs.type),
	data(std::move(rhs.data)),
	starting_address(rhs.starting_address),
	entry_address(rhs.entry_address) {}

File::File() :
	type(Type::Binary),
	starting_address(0),
	entry_address(0) {}	// For the sake of initialising in a defined state.

std::vector<File> Analyser::Static::MSX::GetFiles(const std::shared_ptr<Storage::Tape::Tape> &tape) {
	std::vector<File> files;

	Storage::Tape::BinaryTapePlayer tape_player(1000000);
	tape_player.set_motor_control(true);
	tape_player.set_tape(tape);

	using Parser = Storage::Tape::MSX::Parser;

	// Get all recognisable files from the tape.
	while(!tape->is_at_end()) {
		// Try to locate and measure a header.
		std::unique_ptr<Parser::FileSpeed> file_speed = Parser::find_header(tape_player);
		if(!file_speed) continue;

		// Check whether what follows is a recognisable file type.
		uint8_t header[10] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
		for(std::size_t c = 0; c < sizeof(header); ++c) {
			int next_byte = Parser::get_byte(*file_speed, tape_player);
			if(next_byte == -1) break;
			header[c] = uint8_t(next_byte);
		}

		bool bytes_are_same = true;
		for(std::size_t c = 1; c < sizeof(header); ++c)
			bytes_are_same &= (header[c] == header[0]);

		if(!bytes_are_same) continue;
		if(header[0] != 0xd0 && header[0] != 0xd3 && header[0] != 0xea) continue;

		File file;

		// Determine file type from information already collected.
		switch(header[0]) {
			case 0xd0:	file.type = File::Type::Binary;			break;
			case 0xd3:	file.type = File::Type::TokenisedBASIC;	break;
			case 0xea:	file.type = File::Type::ASCII;			break;
			default: break;	// Unreachable.
		}

		// Read file name.
		char name[7];
		for(std::size_t c = 1; c < 6; ++c)
			name[c] = char(Parser::get_byte(*file_speed, tape_player));
		name[6] = '\0';
		file.name = name;

		// ASCII: Read 256-byte segments until one ends with an end-of-file character.
		if(file.type == File::Type::ASCII) {
			while(true) {
				file_speed = Parser::find_header(tape_player);
				if(!file_speed) break;
				int c = 256;
				bool contains_end_of_file = false;
				while(c--) {
					int byte = Parser::get_byte(*file_speed, tape_player);
					if(byte == -1) break;
					contains_end_of_file |= (byte == 0x1a);
					file.data.push_back(uint8_t(byte));
				}
				if(c != -1) break;
				if(contains_end_of_file) {
					files.push_back(std::move(file));
					break;
				}
			}
			continue;
		}

		// Read a single additional segment, using the information at the begging to determine length.
		file_speed = Parser::find_header(tape_player);
		if(!file_speed) continue;

		// Binary: read start address, end address, entry address, then that many bytes.
		if(file.type == File::Type::Binary) {
			uint8_t locations[6];
			uint16_t end_address;
			std::size_t c;
			for(c = 0; c < sizeof(locations); ++c) {
				int byte = Parser::get_byte(*file_speed, tape_player);
				if(byte == -1) break;
				locations[c] = uint8_t(byte);
			}
			if(c != sizeof(locations)) continue;

			file.starting_address = uint16_t(locations[0] | (locations[1] << 8));
			end_address = uint16_t(locations[2] | (locations[3] << 8));
			file.entry_address = uint16_t(locations[4] | (locations[5] << 8));

			if(end_address < file.starting_address) continue;

			std::size_t length = end_address - file.starting_address;
			while(length--) {
				int byte = Parser::get_byte(*file_speed, tape_player);
				if(byte == -1) continue;
				file.data.push_back(uint8_t(byte));
			}

			files.push_back(std::move(file));
			continue;
		}

		// Tokenised BASIC, then: keep following 'next line' links from a hypothetical start of
		// 0x8001, until finding the final line.
		uint16_t current_address = 0x8001;
		while(current_address) {
			int next_address_buffer[2];
			next_address_buffer[0] = Parser::get_byte(*file_speed, tape_player);
			next_address_buffer[1] = Parser::get_byte(*file_speed, tape_player);

			if(next_address_buffer[0] == -1 || next_address_buffer[1] == -1) break;
			file.data.push_back(uint8_t(next_address_buffer[0]));
			file.data.push_back(uint8_t(next_address_buffer[1]));

			uint16_t next_address = uint16_t(next_address_buffer[0] | (next_address_buffer[1] << 8));
			if(!next_address) {
				files.push_back(std::move(file));
				break;
			}
			if(next_address < current_address+2) break;

			// This line makes sense, so push it all in.
			std::size_t length = next_address - current_address - 2;
			current_address = next_address;
			bool found_error = false;
			while(length--) {
				int byte = Parser::get_byte(*file_speed, tape_player);
				if(byte == -1) {
					found_error = true;
					break;
				}
				file.data.push_back(uint8_t(byte));
			}
			if(found_error) break;
		}
	}

	return files;
}
