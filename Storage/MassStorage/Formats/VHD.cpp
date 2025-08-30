//
//  VHD.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 27/08/2025.
//  Copyright © 2025 Thomas Harte. All rights reserved.
//

#include "VHD.hpp"

#include <cassert>

using namespace Storage::MassStorage;

namespace {
constexpr size_t SectorSize = 512;
}

VHD::VHD(const std::string &file_name) : file_(file_name) {
	// Find footer; this may be the final 511 or final 512 bytes of the file.
	// Find what might be the start of the 'conectix' [sic] signature.
	file_.seek(-511, Whence::END);
	const auto c = file_.get();
	switch(c) {
		case 'c':	file_.seek(-511, Whence::END);	break;
		case 'o':	file_.seek(-512, Whence::END);	break;
		default:	throw std::exception();
	}

	if(!file_.check_signature<SignatureType::String>("conectix")) {
		throw std::exception();
	}

	file_.seek(4, Whence::CUR);	// Skip 'Features', which would at best classify this disk as temporary or not.
	const auto major_version = file_.get_be<uint16_t>();
	if(major_version > 1) {
		throw std::exception();
	}
	file_.seek(2, Whence::CUR);	// Skip minor version number.

	data_offset_ = file_.get_be<uint64_t>();

	file_.seek(32, Whence::CUR);	// Skip creator and timestamp fields, original size and current size.
//	const auto current_size = file_.get_be<uint64_t>();

	cylinders_ = file_.get_be<uint16_t>();
	heads_ = file_.get();
	sides_ = file_.get();

	switch(file_.get_be<uint32_t>()) {
		case 2:	type_ = Type::Fixed;		break;
		case 3:	type_ = Type::Dynamic;		break;
		case 4:	type_ = Type::Differencing;	break;

		default:
			throw std::exception();
	}

	if(type_ != Type::Dynamic) {
		total_blocks_ = cylinders_ * heads_ * sides_;
		return;
	}

	file_.seek(static_cast<long>(data_offset_), Whence::SET);
	if(!file_.check_signature<SignatureType::String>("cxsparse")) {
		throw std::exception();
	}

	file_.seek(8, Whence::CUR);	// Skip data offset.
	table_offset_ = file_.get_be<uint64_t>();

	file_.seek(4, Whence::CUR);	// Skip table version. TODO: validate.
	max_table_entries_ = file_.get_be<uint32_t>();
	block_size_ = file_.get_be<uint32_t>();

	total_blocks_ = block_size_ * max_table_entries_;
}

size_t VHD::get_block_size() const {
	return SectorSize;
}

size_t VHD::get_number_of_blocks() const {
	return total_blocks_;
}

std::vector<uint8_t> VHD::get_block(size_t) const {
	// TODO.
	assert(false);
	return std::vector<uint8_t>(SectorSize);
}

void VHD::set_block(size_t, const std::vector<uint8_t> &) {
	// TODO.
	assert(false);
}
