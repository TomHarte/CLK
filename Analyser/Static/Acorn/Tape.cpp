//
//  Tape.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 29/08/2016.
//  Copyright 2016 Thomas Harte. All rights reserved.
//

#include "Tape.hpp"

#include <deque>

#include "../../../Numeric/CRC.hpp"
#include "../../../Storage/Tape/Parsers/Acorn.hpp"

using namespace Analyser::Static::Acorn;

static std::unique_ptr<File::Chunk> GetNextChunk(const std::shared_ptr<Storage::Tape::Tape> &tape, Storage::Tape::Acorn::Parser &parser) {
	auto new_chunk = std::make_unique<File::Chunk>();
	int shift_register = 0;

// TODO: move this into the parser
#define shift()	shift_register = (shift_register >> 1) |  (parser.get_next_bit(tape) << 9)

	// find next area of high tone
	while(!tape->is_at_end() && (shift_register != 0x3ff)) {
		shift();
	}

	// find next 0x2a (swallowing stop bit)
	while(!tape->is_at_end() && (shift_register != 0x254)) {
		shift();
	}

#undef shift

	parser.reset_crc();
	parser.reset_error_flag();

	// read out name
	char name[11];
	std::size_t name_ptr = 0;
	while(!tape->is_at_end() && name_ptr < sizeof(name)) {
		name[name_ptr] = char(parser.get_next_byte(tape));
		if(!name[name_ptr]) break;
		name_ptr++;
	}
	name[sizeof(name)-1] = '\0';
	new_chunk->name = name;

	// addresses
	new_chunk->load_address = uint32_t(parser.get_next_word(tape));
	new_chunk->execution_address = uint32_t(parser.get_next_word(tape));
	new_chunk->block_number = uint16_t(parser.get_next_short(tape));
	new_chunk->block_length = uint16_t(parser.get_next_short(tape));
	new_chunk->block_flag = uint8_t(parser.get_next_byte(tape));
	new_chunk->next_address = uint32_t(parser.get_next_word(tape));

	uint16_t calculated_header_crc = parser.get_crc();
	uint16_t stored_header_crc = uint16_t(parser.get_next_short(tape));
	stored_header_crc = uint16_t((stored_header_crc >> 8) | (stored_header_crc << 8));
	new_chunk->header_crc_matched = stored_header_crc == calculated_header_crc;

	if(!new_chunk->header_crc_matched) return nullptr;

	parser.reset_crc();
	new_chunk->data.reserve(new_chunk->block_length);
	for(int c = 0; c < new_chunk->block_length; c++) {
		new_chunk->data.push_back(uint8_t(parser.get_next_byte(tape)));
	}

	if(new_chunk->block_length && !(new_chunk->block_flag&0x40)) {
		uint16_t calculated_data_crc = parser.get_crc();
		uint16_t stored_data_crc = uint16_t(parser.get_next_short(tape));
		stored_data_crc = uint16_t((stored_data_crc >> 8) | (stored_data_crc << 8));
		new_chunk->data_crc_matched = stored_data_crc == calculated_data_crc;
	} else {
		new_chunk->data_crc_matched = true;
	}

	return parser.get_error_flag() ? nullptr : std::move(new_chunk);
}

static std::unique_ptr<File> GetNextFile(std::deque<File::Chunk> &chunks) {
	// find next chunk with a block number of 0
	while(chunks.size() && chunks.front().block_number) {
		chunks.pop_front();
	}

	if(!chunks.size()) return nullptr;

	// accumulate chunks for as long as block number is sequential and the end-of-file bit isn't set
	auto file = std::make_unique<File>();

	uint16_t block_number = 0;

	while(chunks.size()) {
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
	for(File::Chunk chunk : file->chunks) {
		file->data.insert(file->data.end(), chunk.data.begin(), chunk.data.end());
	}

	return file;
}

std::vector<File> Analyser::Static::Acorn::GetFiles(const std::shared_ptr<Storage::Tape::Tape> &tape) {
	Storage::Tape::Acorn::Parser parser;

	// populate chunk list
	std::deque<File::Chunk> chunk_list;
	while(!tape->is_at_end()) {
		std::unique_ptr<File::Chunk> chunk = GetNextChunk(tape, parser);
		if(chunk) {
			chunk_list.push_back(*chunk);
		}
	}

	// decompose into file list
	std::vector<File> file_list;

	while(chunk_list.size()) {
		std::unique_ptr<File> next_file = GetNextFile(chunk_list);
		if(next_file) {
			file_list.push_back(*next_file);
		}
	}

	return file_list;
}
