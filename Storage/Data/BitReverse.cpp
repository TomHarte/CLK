//
//  BitReverse.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 03/10/2017.
//  Copyright 2017 Thomas Harte. All rights reserved.
//

#include "BitReverse.hpp"

void Storage::Data::BitReverse::reverse(std::vector<uint8_t> &vector) {
	for(auto &byte : vector) {
		byte =
			uint8_t(
				((byte & 0x01) << 7) |
				((byte & 0x02) << 5) |
				((byte & 0x04) << 3) |
				((byte & 0x08) << 1) |
				((byte & 0x10) >> 1) |
				((byte & 0x20) >> 3) |
				((byte & 0x40) >> 5) |
				((byte & 0x80) >> 7)
			);
	}
}
