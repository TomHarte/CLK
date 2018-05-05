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

		bool operator < (const Address &rhs) const {
			return ((track << 24) | (side << 8) | sector) < ((rhs.track << 24) | (rhs.side << 8) | rhs.sector);
		}
	};

	Address address;
	uint8_t size = 0;	// Size is stored in ordinary MFM form — the number of bytes included in this sector
						// is 2^(7 + size), or 128 << size.

	// Multiple samplings of the underlying data are accepted, to allow weak and fuzzy data to be communicated.
	std::vector<std::vector<uint8_t>> samples;

	bool has_data_crc_error = false;
	bool has_header_crc_error = false;
	bool is_deleted = false;

	Sector() noexcept {}

	Sector(const Sector &&rhs) noexcept :
		address(rhs.address),
		size(rhs.size),
		samples(std::move(rhs.samples)),
		has_data_crc_error(rhs.has_data_crc_error),
		has_header_crc_error(rhs.has_header_crc_error),
		is_deleted(rhs.is_deleted ){}
};

}
}
}

#endif /* Sector_h */
