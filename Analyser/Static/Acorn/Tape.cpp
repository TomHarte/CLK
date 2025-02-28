//
//  Tape.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 29/08/2016.
//  Copyright 2016 Thomas Harte. All rights reserved.
//

#include "Tape.hpp"

#include <deque>

#include "Numeric/CRC.hpp"
#include "Storage/Tape/Parsers/Acorn.hpp"

using namespace Analyser::Static::Acorn;

static std::unique_ptr<File::Chunk> GetNextChunk(
	Storage::Tape::TapeSerialiser &serialiser,
	Storage::Tape::Acorn::Parser &parser
) {
	auto new_chunk = std::make_unique<File::Chunk>();
	int shift_register = 0;

	// TODO: move this into the parser
	const auto find = [&](int target) {
		while(!serialiser.is_at_end() && (shift_register != target)) {
			shift_register = (shift_register >> 1) | (parser.get_next_bit(serialiser) << 9);
		}
	};

	// Find first sync byte that follows high tone.
	find(0x3ff);
	find(0x254);	// i.e. 0x2a wrapped in a 1 start bit and a 0 stop bit.
	parser.reset_crc();
	parser.reset_error_flag();

	// Read name.
	char name[11]{};
	std::size_t name_ptr = 0;
	while(!serialiser.is_at_end() && name_ptr < sizeof(name)) {
		name[name_ptr] = char(parser.get_next_byte(serialiser));
		if(!name[name_ptr]) break;
		++name_ptr;
	}
	new_chunk->name = name;

	// Read rest of header fields.
	new_chunk->load_address = uint32_t(parser.get_next_word(serialiser));
	new_chunk->execution_address = uint32_t(parser.get_next_word(serialiser));
	new_chunk->block_number = uint16_t(parser.get_next_short(serialiser));
	new_chunk->block_length = uint16_t(parser.get_next_short(serialiser));
	new_chunk->block_flag = uint8_t(parser.get_next_byte(serialiser));
	new_chunk->next_address = uint32_t(parser.get_next_word(serialiser));

	const auto matched_crc = [&]() {
		const uint16_t calculated_crc = parser.get_crc();
		uint16_t stored_crc = uint16_t(parser.get_next_short(serialiser));
		stored_crc = uint16_t((stored_crc >> 8) | (stored_crc << 8));
		return stored_crc == calculated_crc;
	};

	new_chunk->header_crc_matched = matched_crc();

	if(!new_chunk->header_crc_matched) return nullptr;

	// Bit 6 of the block flag means 'empty block'; allow it to override declared block length.
	if(new_chunk->block_length && !(new_chunk->block_flag&0x40)) {
		parser.reset_crc();
		new_chunk->data.reserve(new_chunk->block_length);
		for(int c = 0; c < new_chunk->block_length; c++) {
			new_chunk->data.push_back(uint8_t(parser.get_next_byte(serialiser)));
		}
		new_chunk->data_crc_matched = matched_crc();
	} else {
		new_chunk->data_crc_matched = true;
	}

	return parser.get_error_flag() ? nullptr : std::move(new_chunk);
}

static std::unique_ptr<File> GetNextFile(std::deque<File::Chunk> &chunks) {
	// Find next chunk with a block number of 0.
	while(!chunks.empty() && chunks.front().block_number) {
		chunks.pop_front();
	}
	if(chunks.empty()) return nullptr;

	// Accumulate sequential blocks until end-of-file bit is set.
	auto file = std::make_unique<File>();
	uint16_t block_number = 0;
	while(!chunks.empty()) {
		if(chunks.front().block_number != block_number) return nullptr;

		const bool was_last = chunks.front().block_flag & 0x80;
		file->chunks.push_back(chunks.front());
		chunks.pop_front();
		++block_number;

		if(was_last) break;
	}

	// Grab metadata flags.
	file->name = file->chunks.front().name;
	file->load_address = file->chunks.front().load_address;
	file->execution_address = file->chunks.front().execution_address;
	if(file->chunks.back().block_flag & 0x01) {
		// File is locked i.e. for execution only.
		file->flags |= File::Flags::ExecuteOnly;
	}

	// Copy data into a single big block.
	file->data.reserve(file->chunks.size() * 256);
	for(auto &chunk : file->chunks) {
		file->data.insert(file->data.end(), chunk.data.begin(), chunk.data.end());
	}

	return file;
}

std::vector<File> Analyser::Static::Acorn::GetFiles(Storage::Tape::TapeSerialiser &serialiser) {
	Storage::Tape::Acorn::Parser parser;

	// Read all chunks.
	std::deque<File::Chunk> chunk_list;
	while(!serialiser.is_at_end()) {
		const std::unique_ptr<File::Chunk> chunk = GetNextChunk(serialiser, parser);
		if(chunk) {
			chunk_list.push_back(std::move(*chunk));
		}
	}

	// Convert to files.
	std::vector<File> file_list;
	while(!chunk_list.empty()) {
		const std::unique_ptr<File> next_file = GetNextFile(chunk_list);
		if(next_file) {
			file_list.push_back(std::move(*next_file));
		}
	}

	return file_list;
}
