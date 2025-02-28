//
//  BinaryDump.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 28/08/2016.
//  Copyright 2016 Thomas Harte. All rights reserved.
//

#include "BinaryDump.hpp"
#include "Storage/FileHolder.hpp"

using namespace Storage::Cartridge;

BinaryDump::BinaryDump(const std::string &file_name) {
	auto contents = Storage::contents_of(file_name);
	segments_.emplace_back(
		::Storage::Cartridge::Cartridge::Segment::UnknownAddress,
		::Storage::Cartridge::Cartridge::Segment::UnknownAddress,
		std::move(contents));
}
