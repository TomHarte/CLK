//
//  Sector.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 04/05/2018.
//  Copyright © 2018 Thomas Harte. All rights reserved.
//

#ifndef Sector_h
#define Sector_h

#include <cstdint>
#include <vector>

namespace Storage {
namespace Encodings {
namespace AppleGCR {

struct Sector {
	/*!
		Describes the location of a sector, implementing < to allow for use as a set key.
	*/
	struct Address {
		uint_fast8_t volume = 0, track = 0, sector = 0;

		bool operator < (const Address &rhs) const {
			return ((volume << 16) | (track << 8) | sector) < ((rhs.volume << 16) | (rhs.track << 8) | rhs.sector);
		}
	};

	Address address;
	std::vector<uint8_t> data;

	bool has_data_checksum_error = false;
	bool has_header_checksum_error = false;

	enum class Encoding {
		FiveAndThree, SixAndTwo
	};
	Encoding encoding = Encoding::SixAndTwo;
};

}
}
}

#endif /* Sector_h */
