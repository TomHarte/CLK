//
//  SegmentParser.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 04/05/2018.
//  Copyright 2018 Thomas Harte. All rights reserved.
//

#include "SegmentParser.hpp"

#include "Encoder.hpp"

#include <array>

namespace {

const uint8_t six_and_two_unmapping[] = {
	0x00, 0x01, 0xff, 0xff,
	0x02, 0x03, 0xff, 0x04, 0x05, 0x06, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0x07, 0x08, 0xff, 0xff,
	0xff, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0xff, 0xff,
	0x0e, 0x0f, 0x10, 0x11, 0x12, 0x13, 0xff, 0x14,
	0x15, 0x16, 0x17, 0x18, 0x19, 0x1a, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0x1b, 0xff, 0x1c, 0x1d, 0x1e, 0xff, 0xff,
	0xff, 0x1f, 0xff, 0xff, 0x20, 0x21, 0xff, 0x22,
	0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0xff, 0xff,
	0xff, 0xff, 0xff, 0x29, 0x2a, 0x2b, 0xff, 0x2c,
	0x2d, 0x2e, 0x2f, 0x30, 0x31, 0x32, 0xff, 0xff,
	0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0xff, 0x39,
	0x3a, 0x3b, 0x3c, 0x3d, 0x3e, 0x3f, 0xff, 0xff
};

}

using namespace Storage::Encodings::AppleGCR;

std::map<std::size_t, Sector> Storage::Encodings::AppleGCR::sectors_from_segment(const Disk::PCMSegment &&segment) {
	std::map<std::size_t, Sector> result;

	uint_fast8_t shift_register = 0;

	const std::size_t scanning_sentinel = std::numeric_limits<std::size_t>::max();
	std::unique_ptr<Sector> new_sector;
	std::size_t sector_location = 0;
	std::size_t pointer = scanning_sentinel;
	std::array<uint_fast8_t, 8> header{{0, 0, 0, 0, 0, 0, 0, 0}};
	std::array<uint_fast8_t, 3> scanner{{0, 0, 0}};

	// Scan the track 1 and 1/8th times; that's long enough to make sure that any sector which straddles the
	// end of the track is caught. Since they're put into a map, it doesn't matter if they're caught twice.
	const size_t extended_length = segment.data.size() + (segment.data.size() >> 3);
	for(size_t bit = 0; bit < extended_length; ++bit) {
		shift_register = static_cast<uint_fast8_t>((shift_register << 1) | (segment.data[bit % segment.data.size()] ? 1 : 0));

		// Apple GCR parsing: bytes always have the top bit set.
		if(!(shift_register&0x80)) continue;

		// Grab the byte.
		const uint_fast8_t value = shift_register;
		shift_register = 0;

		scanner[0] = scanner[1];
		scanner[1] = scanner[2];
		scanner[2] = value;

		if(pointer == scanning_sentinel) {
			if(
				scanner[0] == header_prologue[0] &&
				scanner[1] == header_prologue[1] &&
				(
					scanner[2] == header_prologue[2] ||
					scanner[2] == data_prologue[2]
				)) {
				pointer = 0;

				// If this is the start of a data section, and at least
				// one header has been witnessed, start a sector.
				if(scanner[2] == data_prologue[2]) {
					new_sector.reset(new Sector);
					new_sector->data.reserve(412);
				} else {
					sector_location = static_cast<std::size_t>(bit % segment.data.size());
				}
			}
		} else {
			if(new_sector) {
				new_sector->data.push_back(value);

				// If this is potentially a complete sector, check it out.
				if(new_sector->data.size() == 343) {
					// TODO: allow for 13-sector form.
					std::unique_ptr<Sector> sector = std::move(new_sector);
					new_sector.reset();
					pointer = scanning_sentinel;

					// Check for apparent four and four encoding.
					uint_fast8_t header_mask = 0xff;
					for(auto c : header) header_mask &= c;
					header_mask &= 0xaa;
					if(header_mask != 0xaa) continue;

					sector->address.volume = ((header[0] << 1) | 1) & header[1];
					sector->address.track = ((header[2] << 1) | 1) & header[3];
					sector->address.sector = ((header[4] << 1) | 1) & header[5];

					// Check the header checksum.
					uint_fast8_t checksum = ((header[6] << 1) | 1) & header[7];
					if(checksum != (sector->address.volume^sector->address.track^sector->address.sector)) continue;

					// Unmap the sector contents as 6 and 2 data.
					bool out_of_bounds = false;
					for(auto &c : sector->data) {
						if(c < 0x96 || six_and_two_unmapping[c - 0x96] == 0xff) {
							out_of_bounds = true;
							break;
						}
						c = six_and_two_unmapping[c - 0x96];
					}
					if(out_of_bounds) continue;

					// Undo the XOR step on sector contents and check that checksum.
					for(std::size_t c = 1; c < sector->data.size(); ++c) {
						sector->data[c] ^= sector->data[c-1];
					}
					if(sector->data.back()) continue;

					// Having checked the checksum, remove it.
					sector->data.resize(sector->data.size() - 1);

					// Undo the 6 and 2 mapping.
					const uint8_t bit_reverse[] = {0, 2, 1, 3};
					#define unmap(byte, nibble, shift)	\
						sector->data[86 + byte] = static_cast<uint8_t>(\
							(sector->data[86 + byte] << 2) | bit_reverse[(sector->data[nibble] >> shift)&3]);

					for(std::size_t c = 0; c < 84; ++c) {
						unmap(c, c, 0);
						unmap(c+86, c, 2);
						unmap(c+172, c, 4);
					}

					unmap(84, 84, 0);
					unmap(170, 84, 2);
					unmap(85, 85, 0);
					unmap(171, 85, 2);

					#undef unmap

					// Throw away the collection of two-bit chunks.
					sector->data.erase(sector->data.begin(), sector->data.end() - 256);

					// Add this sector to the map.
					result.insert(std::make_pair(sector_location, std::move(*sector)));
				}
			} else {
				// Just capture the header in place; it'll be decoded
				// once a whole sector has been read.
				header[pointer] = value;
				++pointer;
				if(pointer == 8) {
					pointer = scanning_sentinel;
				}
			}
		}
	}

	return result;
}
