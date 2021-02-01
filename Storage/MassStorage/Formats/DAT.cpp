//
//  DAT.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 31/01/2021.
//  Copyright © 2021 Thomas Harte. All rights reserved.
//

#include "DAT.hpp"

using namespace Storage::MassStorage;

DAT::DAT(const std::string &file_name) : file_(file_name) {
	// Is the file a multiple of 256 bytes in size?
	const auto file_size = file_.stats().st_size;
	if(file_size & 255) throw std::exception();

	// Does it contain the 'Hugo' signature?
	file_.seek(0x201, SEEK_SET);
	if(!file_.check_signature("Hugo")) {
		throw std::exception();
	}
}

size_t DAT::get_block_size() {
	return 256;
}

size_t DAT::get_number_of_blocks() {
	return size_t(file_.stats().st_size) / 256;
}

std::vector<uint8_t> DAT::get_block(size_t address) {
	file_.seek(long(address * 256), SEEK_SET);
	return file_.read(256);
}

void DAT::set_block(size_t address, const std::vector<uint8_t> &contents) {
	file_.seek(long(address * 256), SEEK_SET);
	file_.write(contents);
}
