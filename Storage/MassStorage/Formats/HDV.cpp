//
//  HDV.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 14/11/2022.
//  Copyright © 2022 Thomas Harte. All rights reserved.
//

#include "HDV.hpp"

#include <algorithm>

using namespace Storage::MassStorage;

HDV::HDV(const std::string_view file_name, const long start, const long size):
	file_(file_name),
	file_start_(start),
	image_size_(std::min(size, long(file_.stats().st_size)))
{
	mapper_.set_drive_type(
		Storage::MassStorage::Encodings::Apple::DriveType::SCSI,
		size_t(size / 512)
	);
}

size_t HDV::get_block_size() const {
	return 512;
}

size_t HDV::get_number_of_blocks() const {
	return mapper_.get_number_of_blocks();
}

std::vector<uint8_t> HDV::get_block(const size_t address) const {
	const auto source_address = mapper_.to_source_address(address);
	const auto file_offset = offset_for_block(source_address);

	if(source_address >= 0) {
		file_.seek(file_offset, Whence::SET);
		return mapper_.convert_source_block(source_address, file_.read(get_block_size()));
	} else {
		return mapper_.convert_source_block(source_address);
	}
}

void HDV::set_block(const size_t address, const std::vector<uint8_t> &data) {
	const auto source_address = mapper_.to_source_address(address);
	const auto file_offset = offset_for_block(source_address);

	if(source_address >= 0 && file_offset >= 0) {
		file_.seek(file_offset, Whence::SET);
		file_.write(data);
	}
}

long HDV::offset_for_block(ssize_t address) const {
	if(address < 0) return -1;

	const long offset = 512 * address;
	if(offset > image_size_ - 512) return -1;

	return file_start_ + offset;
}
