//
//  SAP.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 20/04/2026.
//  Copyright © 2026 Thomas Harte. All rights reserved.
//

#include "SAP.hpp"

using namespace Storage::Disk;

SAP::SAP(const std::string &file_name) : file_(file_name) {
}

HeadPosition SAP::maximum_head_position() const {
	return HeadPosition(2);
}

bool SAP::is_read_only() const {
	return true;
}

bool SAP::represents(const std::string &name) const {
	return name == file_.name();
}

Track::Address SAP::canonical_address(const Track::Address address) const {
	return Track::Address(
		address.head,
		HeadPosition(address.position.as_int())
	);
}

std::unique_ptr<Track> SAP::track_at_position(Track::Address) const {
	return nullptr;
}

void SAP::set_tracks(const std::map<Track::Address, std::unique_ptr<Track>> &) {
}
