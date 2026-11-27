//
//  Addressing.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 26/11/2026.
//  Copyright © 2026 Thomas Harte. All rights reserved.
//

#pragma once

#include "Storage/Storage.hpp"

namespace Storage::Encodings::AppleGCR {

struct Address {
	int side;
	int track;
	int sector;
	int zone;
};

Address address_from_block_35(int);
Storage::Time bit_length_35(int zone);

}
