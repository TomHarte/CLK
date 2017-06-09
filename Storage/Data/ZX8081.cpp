//
//  ZX8081.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 08/06/2017.
//  Copyright © 2017 Thomas Harte. All rights reserved.
//

#include "ZX8081.hpp"

using namespace Storage::Data::ZX8081;

static uint16_t short_at(size_t address, const std::vector<uint8_t> &data) {
	return (uint16_t)(data[address] | (data[address + 1] << 8));
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
	if(end_of_file - 0x4000 > data.size()) return nullptr;

	// check for the proper ordering of buffers
	if(vars > end_of_file) return nullptr;
	if(end_of_file > display_address) return nullptr;

	// TODO: check that the line numbers declared above exist (?)

	std::shared_ptr<File> file(new File);
	file->data = data;
	file->isZX81 = false;
	return file;
}

std::shared_ptr<File> Storage::Data::ZX8081::FileFromData(const std::vector<uint8_t> &data) {
	std::shared_ptr<Storage::Data::ZX8081::File> result = ZX80FileFromData(data);

	return result;
}
