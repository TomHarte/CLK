//
//  DSK.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 01/05/2021.
//  Copyright © 2021 Thomas Harte. All rights reserved.
//

#include "DSK.hpp"

using namespace Storage::MassStorage;

DSK::DSK(const std::string &file_name) : RawSectorDump(file_name) {
	// Minimum validation: check the first sector for a device signature,
	// with 512-byte blocks.
	const auto sector = get_block(0);
	if(sector.size() != 512) {
		throw std::exception();
	}
	if(sector[0] != 0x45 || sector[1] != 0x52 || sector[2] != 0x02 || sector[3] != 0x00) {
		throw std::exception();
	}
}
