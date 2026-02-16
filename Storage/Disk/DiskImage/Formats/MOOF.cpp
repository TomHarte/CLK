//
//  MOOF.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 15/02/2026.
//  Copyright © 2026 Thomas Harte. All rights reserved.
//

#include "MOOF.hpp"

using namespace Storage::Disk;

MOOF::MOOF(const std::string &file_name) :
	file_(file_name) {

	static constexpr char signature[] = {
		'M', 'O', 'O', 'F',
		char(0xff), 0x0a, 0x0d, 0x0a
	};
	if(!file_.check_signature<SignatureType::Binary>(signature)) {
		throw Error::InvalidFormat;
	}
}

HeadPosition MOOF::maximum_head_position() const {
	return HeadPosition(1);
}

std::unique_ptr<Track> MOOF::track_at_position(Track::Address) const {
	return nullptr;
}

bool MOOF::represents(const std::string &name) const {
	return name == file_.name();
}
