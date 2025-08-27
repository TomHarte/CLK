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
