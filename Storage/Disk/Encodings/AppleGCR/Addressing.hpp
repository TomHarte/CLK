//
//  Addressing.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 26/11/2026.
//  Copyright © 2026 Thomas Harte. All rights reserved.
//

#pragma once

namespace Storage::Encodings::AppleGCR {

struct Address {
	int side;
	int track;
	int sector;
};

Address address_from_block_35(int);

}
