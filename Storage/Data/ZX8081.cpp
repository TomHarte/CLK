//
//  ZX8081.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 08/06/2017.
//  Copyright 2017 Thomas Harte. All rights reserved.
//

#include "ZX8081.hpp"

using namespace Storage::Data::ZX8081;

static uint16_t short_at(std::size_t address, const std::vector<uint8_t> &data) {
	return uint16_t(data[address] | (data[address + 1] << 8));
}

static std::shared_ptr<File> ZX80FileFromData(const std::vector<uint8_t> &data) {
	// Does this look like a ZX80 file?

	if(data.size() < 0x28) return nullptr;

//	uint16_t next_line_number = short_at(0x2, data);
//	uint16_t first_visible_line = short_at(0x13, data);

	uint16_t vars = short_at(0x8, data);
	uint16_t end_of_file = short_at(0xa, data);
	uint16_t display_address = short_at(0xc, data);

	// check that the end of file is contained within the supplied data
	if(size_t(end_of_file - 0x4000) > data.size()) return nullptr;

	// check for the proper ordering of buffers
	if(vars > end_of_file) return nullptr;
	if(end_of_file > display_address) return nullptr;

	// TODO: does it make sense to inspect the tokenised BASIC?
	// It starts at 0x4028 and proceeds as [16-bit line number] [tokens] [0x76],
	// but I'm as yet unable to find documentation of the tokens.

	// TODO: check that the line numbers declared above exist (?)

	auto file = std::make_shared<File>();
	file->data = data;
	file->isZX81 = false;
	return file;
}

static std::shared_ptr<File> ZX81FileFromData(const std::vector<uint8_t> &data) {
	// Does this look like a ZX81 file?

	// Look for a file name.
	std::size_t data_pointer = 0;
	std::vector<uint8_t> name_data;
	std::size_t c = 11;
	while(c < data.size() && c--) {
		name_data.push_back(data[data_pointer] & 0x3f);
		if(data[data_pointer] & 0x80) break;
		data_pointer++;
	}
	if(!c) return nullptr;
	data_pointer++;

	if(data.size() < data_pointer + 0x405e - 0x4009) return nullptr;

//	if(data[data_pointer]) return nullptr;

//	uint16_t vars = short_at(data_pointer + 0x4010 - 0x4009, data);
	uint16_t end_of_file = short_at(data_pointer + 0x4014 - 0x4009, data);
//	uint16_t display_address = short_at(0x400c - 0x4009, data);

	// check that the end of file is contained within the supplied data
	if(data_pointer + end_of_file - 0x4009 > data.size()) return nullptr;

	// check for the proper ordering of buffers
//	if(vars > end_of_file) return nullptr;
//	if(end_of_file > display_address) return nullptr;

	// TODO: does it make sense to inspect the tokenised BASIC?
	// It starts at 0x4028 and proceeds as [16-bit line number] [tokens] [0x76],
	// but I'm as yet unable to find documentation of the tokens.

	// TODO: check that the line numbers declared above exist (?)

	auto file = std::make_shared<File>();
	file->name = StringFromData(name_data, true);
	file->data = data;
	file->isZX81 = true;
	return file;
}

std::shared_ptr<File> Storage::Data::ZX8081::FileFromData(const std::vector<uint8_t> &data) {
	std::shared_ptr<Storage::Data::ZX8081::File> result = ZX81FileFromData(data);
	if(result) return result;
	return ZX80FileFromData(data);
}

// MARK: - String conversion

std::wstring Storage::Data::ZX8081::StringFromData(const std::vector<uint8_t> &data, bool is_zx81) {
	std::wstring string;

	wchar_t zx80_map[64] = {
		' ',	u'\u2598',	u'\u259d',	u'\u2580',	u'\u2596',	u'\u258c',	u'\u259e',	u'\u259b',	u'\u2588',	u'\u2584',	u'\u2580',	'"',	u'\u00a3',	'$',	':',	'?',
		'(',	')',		'>',		'<',		'=',		'+',		'-',		'*',		'/',		';',		',',		'.',	'0',		'1',	'2',	'3',
		'4',	'5',		'6',		'7',		'8',		'9',		'A',		'B',		'C',		'D',		'E',		'F',	'G',		'H',	'I',	'J',
		'K',	'L',		'M',		'N',		'O',		'P',		'Q',		'R',		'S',		'T',		'U',		'V',	'W',		'X',	'Y',	'Z'
	};
	// TODO: the block character conversions shown here are in the wrong order
	wchar_t zx81_map[64] = {
		' ',	u'\u2598',	u'\u259d',	u'\u2580',	u'\u2596',	u'\u258c',	u'\u259e',	u'\u259b',	u'\u2588',	u'\u2584',	u'\u2580',	'"',	u'\u00a3',	'$',	':',	'?',
		'(',	')',		'-',		'+',		'*',		'/',		'=',		'>',		'<',		';',		',',		'.',	'0',		'1',	'2',	'3',
		'4',	'5',		'6',		'7',		'8',		'9',		'A',		'B',		'C',		'D',		'E',		'F',	'G',		'H',	'I',	'J',
		'K',	'L',		'M',		'N',		'O',		'P',		'Q',		'R',		'S',		'T',		'U',		'V',	'W',		'X',	'Y',	'Z'
	};
	wchar_t *map = is_zx81 ? zx81_map : zx80_map;

	for(uint8_t byte : data) {
		string.push_back(map[byte & 0x3f]);
	}

	return string;
}

std::vector<uint8_t> Storage::Data::ZX8081::DataFromString(const std::wstring &string, bool is_zx81) {
	std::vector<uint8_t> data;

	// TODO
	(void)string;
	(void)is_zx81;

	return data;
}
