//
//  PRG.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 27/08/2016.
//  Copyright 2016 Thomas Harte. All rights reserved.
//

#include "PRG.hpp"
#include "Storage/FileHolder.hpp"
#include "Storage/Cartridge/Encodings/CommodoreROM.hpp"

using namespace Storage::Cartridge;

PRG::PRG(const std::string &file_name) {
	Storage::FileHolder file(file_name.c_str(), FileMode::Read);

	const auto loading_address = file.get_le<uint16_t>();
	if(loading_address != 0xa000) {
		throw ErrorNotROM;
	}

	const auto data_length = size_t(file.stats().st_size) - 2;
	if(data_length > 0x2000) {
		throw ErrorNotROM;
	}

	// Pad up to a power of two.
	std::size_t padded_data_length = 1;
	while(padded_data_length < data_length) padded_data_length <<= 1;

	std::vector<uint8_t> contents = file.read(data_length);
	contents.resize(padded_data_length);
	if(!Storage::Cartridge::Encodings::CommodoreROM::isROM(contents)) {
		throw ErrorNotROM;
	}

	segments_.emplace_back(0xa000, 0xa000 + data_length, std::move(contents));
}
