//
//  Sector.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 04/05/2018.
//  Copyright 2018 Thomas Harte. All rights reserved.
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
		struct {
			/// For Apple II-type sectors, provides the volume number.
			uint_fast8_t volume = 0;
			/// For Macintosh-type sectors, provides the type from the sector header.
			uint_fast8_t type = 0;
		};
		uint_fast8_t track = 0;
		uint_fast8_t sector = 0;

		/// Filled in for Macintosh sectors only; always @c false for Apple II sectors.
		bool is_side_two = false;

		bool operator < (const Address &rhs) const {
			return (
				((is_side_two ? 1 : 0) << 24) |
				(volume << 16) |
				(track << 8) |
				sector
			) < (
				((rhs.is_side_two ? 1 : 0) << 24) |
				(rhs.volume << 16) |
				(rhs.track << 8) |
				rhs.sector
			);
		}
	};

	Address address;
	std::vector<uint8_t> data;

	bool has_data_checksum_error = false;
	bool has_header_checksum_error = false;

	enum class Encoding {
		FiveAndThree, SixAndTwo, Macintosh
	};
	Encoding encoding = Encoding::SixAndTwo;

	Sector() {}

	Sector(Sector &&rhs) :
		address(rhs.address),
		data(std::move(rhs.data)),
		has_data_checksum_error(rhs.has_data_checksum_error),
		has_header_checksum_error(rhs.has_header_checksum_error),
		encoding(rhs.encoding) {}

	Sector(const Sector &rhs) :
		address(rhs.address),
		data(rhs.data),
		has_data_checksum_error(rhs.has_data_checksum_error),
		has_header_checksum_error(rhs.has_header_checksum_error),
		encoding(rhs.encoding) {}
};

}
}
}

#endif /* Sector_h */
