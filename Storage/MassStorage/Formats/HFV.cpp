//
//  HFV.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 25/08/2019.
//  Copyright © 2019 Thomas Harte. All rights reserved.
//

#include "HFV.hpp"

using namespace Storage::MassStorage;

HFV::HFV(const std::string &file_name) : file_(file_name) {
	// Is the file a multiple of 512 bytes in size and larger than a floppy disk?
	const auto file_size = file_.stats().st_size;
	if(file_size & 511 || file_size <= 800*1024) throw std::exception();

	// TODO: check filing system for MFS, HFS or HFS+.
}

size_t HFV::get_block_size() {
	return 512;
}

size_t HFV::get_number_of_blocks() {
	return size_t(file_.stats().st_size) / get_block_size();
}

std::vector<uint8_t> HFV::get_block(size_t address) {
	const long file_offset = long(get_block_size() * address);
	file_.seek(file_offset, SEEK_SET);
	return file_.read(get_block_size());
}
