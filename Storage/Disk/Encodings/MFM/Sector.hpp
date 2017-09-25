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
	/*!
		Describes the location of a sector, implementing < to allow for use as a set key.
	*/
	struct Address {
		uint8_t track = 0, side = 0, sector = 0;

		bool operator < (Address &rhs) {
			return ((track << 24) | (side << 8) | sector) < ((rhs.track << 24) | (rhs.side << 8) | rhs.sector);
		}
	};

	Address address;
	uint8_t size = 0;
	std::vector<uint8_t> data;

	bool has_data_crc_error = false;
	bool has_header_crc_error = false;
	bool is_deleted = false;
};

}
}
}

#endif /* Sector_h */
