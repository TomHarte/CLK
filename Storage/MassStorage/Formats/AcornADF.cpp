//
//  AcornADF.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 31/01/2021.
//  Copyright © 2021 Thomas Harte. All rights reserved.
//

#include "AcornADF.hpp"

using namespace Storage::MassStorage;

AcornADF::AcornADF(const std::string &file_name) : file_(file_name) {
	// Only one sanity check: is the file a multiple of 256 bytes in size?
	// [TODO: and larger than a floppy disk?]
	const auto file_size = file_.stats().st_size;
	if(file_size & 255) throw std::exception();
}

size_t AcornADF::get_block_size() {
	return 256;
}

size_t AcornADF::get_number_of_blocks() {
	return size_t(file_.stats().st_size) / 256;
}

std::vector<uint8_t> AcornADF::get_block(size_t address) {
	file_.seek(long(address * 256), SEEK_SET);
	return file_.read(256);
}

void AcornADF::set_block(size_t address, const std::vector<uint8_t> &contents) {
	file_.seek(long(address * 256), SEEK_SET);
	file_.write(contents);
}
