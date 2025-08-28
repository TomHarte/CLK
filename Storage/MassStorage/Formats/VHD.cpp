//
//  VHD.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 27/08/2025.
//  Copyright © 2025 Thomas Harte. All rights reserved.
//

#include "VHD.hpp"

using namespace Storage::MassStorage;

VHD::VHD(const std::string &file_name) : file_(file_name) {
	// Find footer; this may be the final 511 or final 512 bytes of the file.
	// Find what might be the start of the 'conectix' [sic] signature.
	file_.seek(-511, SEEK_END);
	const auto c = file_.get();
	switch(c) {
		case 'c':	file_.seek(-511, SEEK_END);	break;
		case 'o':	file_.seek(-512, SEEK_END);	break;
		default:	throw std::exception();
	}

	if(!file_.check_signature("conectix")) {
		throw std::exception();
	}

	file_.seek(4, SEEK_CUR);	// Skip 'Features', which would at best classify this disk as temporary or not.
	const auto major_version = file_.get_be<uint16_t>();
	if(major_version > 1) {
		throw std::exception();
	}
	file_.seek(2, SEEK_CUR);	// Skip minor version number.

	data_offset_ = file_.get_be<uint64_t>();

	file_.seek(32, SEEK_CUR);	// Skip creator and timestamp fields, original size and current size.
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
		return;
	}
	
	printf("");
}

size_t VHD::get_block_size() {
	return 512;
}

size_t VHD::get_number_of_blocks() {
	return 0;
}

std::vector<uint8_t> VHD::get_block(size_t) {
	return {};
}

void VHD::set_block(size_t, const std::vector<uint8_t> &) {
}
