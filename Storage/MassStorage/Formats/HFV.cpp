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

	// Is this an HFS volume?
	// TODO: check filing system for MFS or HFS+.
	const auto prefix = file_.read(2);
	if(prefix[0] != 'L' || prefix[1] != 'K')  throw std::exception();
}

size_t HFV::get_block_size() {
	return 512;
}

size_t HFV::get_number_of_blocks() {
	return mapper_.get_number_of_blocks();
}

std::vector<uint8_t> HFV::get_block(size_t address) {
	const auto written = writes_.find(address);
	if(written != writes_.end()) return written->second;

	const auto source_address = mapper_.to_source_address(address);
	if(source_address >= 0 && size_t(source_address)*get_block_size() < size_t(file_.stats().st_size)) {
		const long file_offset = long(get_block_size()) * long(source_address);
		file_.seek(file_offset, SEEK_SET);
		return mapper_.convert_source_block(source_address, file_.read(get_block_size()));
	} else {
		return mapper_.convert_source_block(source_address);
	}
}

void HFV::set_block(size_t address, const std::vector<uint8_t> &contents) {
	const auto source_address = mapper_.to_source_address(address);
	if(source_address >= 0 && size_t(source_address)*get_block_size() < size_t(file_.stats().st_size)) {
		const long file_offset = long(get_block_size()) * long(source_address);
		file_.seek(file_offset, SEEK_SET);
		file_.write(contents);
	} else {
		writes_[address] = contents;
 	}
}

void HFV::set_drive_type(Encodings::Macintosh::DriveType drive_type) {
	mapper_.set_drive_type(drive_type, size_t(file_.stats().st_size) / get_block_size());
}
