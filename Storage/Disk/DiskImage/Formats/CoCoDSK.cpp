//
//  CoCoDSK.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 20/05/2026.
//  Copyright © 2026 Thomas Harte. All rights reserved.
//

#include "CoCoDSK.hpp"

namespace {
constexpr int SectorsPerTrack = 18;
constexpr int SectorSize = 1;
}

using namespace Storage::Disk;

CoCoDSK::CoCoDSK(const std::string &file_name) : MFMSectorDump(file_name) {
	// Complete validation at present: is this a multiple of the track size?
	if(file_.stats().st_size % (SectorsPerTrack * SectorSize)) throw Error::InvalidFormat;
	set_geometry(SectorsPerTrack, SectorSize, 1, Encodings::MFM::Density::Double);
}

HeadPosition CoCoDSK::maximum_head_position() const {
	return HeadPosition(int(file_.stats().st_size / (SectorsPerTrack * SectorSize)));
}

int CoCoDSK::head_count() const {
	return 1;
}

long CoCoDSK::get_file_offset_for_position(const Track::Address address) const {
	return address.position.as_int() * SectorsPerTrack * (128 << SectorSize);
}
