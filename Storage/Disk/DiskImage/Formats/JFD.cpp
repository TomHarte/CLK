//
//  JFD.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 29/05/2025.
//  Copyright © 2025 Thomas Harte. All rights reserved.
//

#include "JFD.hpp"

using namespace Storage::Disk;

JFD::JFD(const std::string &file_name) : file_name_(file_name) {
	file_ = gzopen(file_name.c_str(), "rb");

	uint8_t signature[4];
	constexpr uint8_t required_signature[4] = { 'J', 'F', 'D', 'I' };
	gzread(file_, signature, sizeof(signature));
	if(!std::equal(std::begin(signature), std::end(signature), std::begin(required_signature))) {
		throw 1;
	}
}

HeadPosition JFD::maximum_head_position() const {
	return HeadPosition{};
}

int JFD::head_count() const {
	return 0;
}

std::unique_ptr<Track> JFD::track_at_position(Track::Address) const {
	return {};
}

bool JFD::represents(const std::string &name) const {
	return name == file_name_;
}
