//
//  Sector.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 24/09/2017.
//  Copyright © 2017 Thomas Harte. All rights reserved.
//

#ifndef Sector_h
#define Sector_h

#include <cstdint>
#include <vector>

namespace Storage {
namespace Encodings {
namespace MFM {

/*!
	Represents a single [M]FM sector, identified by its track, side and sector records, a blob of data
	and a few extra flags of metadata.
*/
struct Sector {
	uint8_t track, side, sector, size;
	std::vector<uint8_t> data;

	bool has_data_crc_error;
	bool has_header_crc_error;
	bool is_deleted;

	Sector() : track(0), side(0), sector(0), size(0), has_data_crc_error(false), has_header_crc_error(false), is_deleted(false) {}
};

}
}
}

#endif /* Sector_h */
