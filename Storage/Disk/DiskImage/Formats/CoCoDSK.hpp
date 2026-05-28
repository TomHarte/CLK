//
//  CoCoDSK.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 20/05/2026.
//  Copyright © 2026 Thomas Harte. All rights reserved.
//

#pragma once

#include "MFMSectorDump.hpp"

namespace Storage::Disk {

class CoCoDSK: public MFMSectorDump {
public:
	CoCoDSK(const std::string &file_name);

	HeadPosition maximum_head_position() const final;
	int head_count() const final;

private:
	long get_file_offset_for_position(Track::Address address) const final;
};

}
