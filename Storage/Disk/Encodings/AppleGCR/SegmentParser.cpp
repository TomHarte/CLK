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
	/* 0x96 */	0x00, 0x01,
	/* 0x98 */	0xff, 0xff, 0x02, 0x03, 0xff, 0x04, 0x05, 0x06,
	/* 0xa0 */	0xff, 0xff,	0xff, 0xff, 0xff, 0xff, 0x07, 0x08,
	/* 0xa8 */	0xff, 0xff, 0xff, 0x09, 0x0a, 0x0b, 0x0c, 0x0d,
	/* 0xb0 */	0xff, 0xff, 0x0e, 0x0f, 0x10, 0x11, 0x12, 0x13,
	/* 0xb8 */	0xff, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1a,
	/* 0xc0 */	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	/* 0xc8 */	0xff, 0xff, 0xff, 0x1b, 0xff, 0x1c, 0x1d, 0x1e,
	/* 0xd0 */	0xff, 0xff, 0xff, 0x1f, 0xff, 0xff, 0x20, 0x21,
	/* 0xd8 */	0xff, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28,
	/* 0xe0 */	0xff, 0xff, 0xff, 0xff, 0xff, 0x29, 0x2a, 0x2b,
	/* 0xe8 */	0xff, 0x2c,	0x2d, 0x2e, 0x2f, 0x30, 0x31, 0x32,
	/* 0xf0 */	0xff, 0xff,	0x33, 0x34, 0x35, 0x36, 0x37, 0x38,
	/* 0xf8 */	0xff, 0x39, 0x3a, 0x3b, 0x3c, 0x3d, 0x3e, 0x3f,
};

uint8_t unmap_six_and_two(uint8_t source) {
	if(source < 0x96) return 0xff;
	return six_and_two_unmapping[source - 0x96];
}

const uint8_t five_and_three_unmapping[] = {
	/* 0xab */	0x00, 0xff,	0x01, 0x02, 0x03,
	/* 0xb0 */	0xff, 0xff, 0xff, 0xff, 0xff, 0x04, 0x05, 0x06,
	/* 0xb8 */	0xff, 0xff, 0x07, 0x08, 0xff, 0x09, 0x0a, 0x0b,
	/* 0xc0 */	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	/* 0xc8 */	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	/* 0xd0 */	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x0c, 0x0d,
	/* 0xd8 */	0xff, 0xff, 0x0e, 0x0f, 0xff, 0x10, 0x11, 0x12,
	/* 0xe0 */	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	/* 0xe8 */	0xff, 0xff,	0x13, 0x14, 0xff, 0x15, 0x16, 0x17,
	/* 0xf0 */	0xff, 0xff,	0xff, 0xff, 0xff, 0x18, 0x19, 0x1a,
	/* 0xf8 */	0xff, 0xff, 0x1b, 0x1c, 0xff, 0x1d, 0x1e, 0x1f,
};

uint8_t unmap_five_and_three(uint8_t source) {
	if(source < 0xab) return 0xff;
	return five_and_three_unmapping[source - 0xab];
}

}

using namespace Storage::Encodings::AppleGCR;

std::map<std::size_t, Sector> Storage::Encodings::AppleGCR::sectors_from_segment(const Disk::PCMSegment &segment) {
	std::map<std::size_t, Sector> result;

	uint_fast8_t shift_register = 0;

	const std::size_t scanning_sentinel = std::numeric_limits<std::size_t>::max();
	std::unique_ptr<Sector> new_sector;
	std::size_t sector_location = 0;
	std::size_t pointer = scanning_sentinel;
	std::array<uint_fast8_t, 8> header{{0, 0, 0, 0, 0, 0, 0, 0}};
	std::array<uint_fast8_t, 3> scanner{{0, 0, 0}};

	// Scan the track while either all bits haven't been seen yet, or a potential
	// sector is still being parsed.
	size_t bit = 0;
	int header_delay = 0;
	bool is_five_and_three = false;
	while(bit < segment.data.size() || pointer != scanning_sentinel || header_delay) {
		shift_register = static_cast<uint_fast8_t>((shift_register << 1) | (segment.data[bit % segment.data.size()] ? 1 : 0));
		++bit;

		// Apple GCR parsing: bytes always have the top bit set.
		if(!(shift_register&0x80)) continue;
		if(header_delay) --header_delay;

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
					scanner[2] == five_and_three_header_prologue[2] ||
					scanner[2] == header_prologue[2] ||
					scanner[2] == data_prologue[2]
				)) {
				pointer = 0;

				if(scanner[2] != data_prologue[2]) {
					is_five_and_three = scanner[2] == five_and_three_header_prologue[2];
				}

				// If this is the start of a data section, and at least
				// one header has been witnessed, start a sector.
				if(scanner[2] == data_prologue[2]) {
					new_sector = std::make_unique<Sector>();
					new_sector->data.reserve(710);
				} else {
					sector_location = static_cast<std::size_t>(bit % segment.data.size());
					header_delay = 200;	// Allow up to 200 bytes to find the body, if the
										// track split comes in between.
				}
			}
		} else {
			if(new_sector) {
				// Check whether the value just read is a legal GCR byte, in six-and-two
				// encoding (which is a strict superset of five-and-three).
				const bool is_invalid = is_five_and_three ? (unmap_five_and_three(value) == 0xff) : (unmap_six_and_two(value) == 0xff);
				if(is_invalid) {
					// The second byte of the standard epilogue is illegal, so this still may
					// be a valid sector. If the final byte was the first byte of an epilogue,
					// chop it off and see whether the sector is otherwise intelligible.

					if(new_sector->data.empty() || new_sector->data.back() != epilogue[0]) {
						// No sector found; reset scanning procedure.
						new_sector.reset();
						pointer = scanning_sentinel;
						continue;
					}

					// Chop off the last byte.
					new_sector->data.resize(new_sector->data.size() - 1);

					// Move the sector elsewhere for processing; there's definitely no way to proceed with
					// the prospective sector if it doesn't parse.
					std::unique_ptr<Sector> sector = std::move(new_sector);
					new_sector.reset();
					pointer = scanning_sentinel;

					// Check for valid decoding options.
					switch(sector->data.size()) {
						default:	// This is not a decodeable sector.
						break;

						case 411:	// Potentially this is an Apple II five-and-three sector.
						case 343: {	// Potentially this is an Apple II six-and-two sector.
							// Check for apparent four and four encoding.
							uint_fast8_t header_mask = 0xff;
							for(auto c : header) header_mask &= c;
							header_mask &= 0xaa;
							if(header_mask != 0xaa) continue;

							sector->address.volume = ((header[0] << 1) | 1) & header[1];
							sector->address.track = ((header[2] << 1) | 1) & header[3];
							sector->address.sector = ((header[4] << 1) | 1) & header[5];

							// Check the header checksum.
							// The 0x11 is reverse engineered from the game 'Alien Rain' and is present even on the boot sector,
							// so probably isn't copy protection?
							uint_fast8_t checksum = (((header[6] << 1) | 1) & header[7]) ^ (is_five_and_three ? 0x11 : 0x00);
							if(checksum != (sector->address.volume^sector->address.track^sector->address.sector)) continue;

							// Unmap the sector contents.
							bool out_of_bounds = false;
							for(auto &c : sector->data) {
								c = is_five_and_three ? unmap_five_and_three(c) : unmap_six_and_two(c);
								if(c == 0xff) {
									out_of_bounds = true;
									break;
								}
							}
							if(out_of_bounds) continue;

							// Undo the XOR step on sector contents and check that checksum.
							for(std::size_t c = 1; c < sector->data.size(); ++c) {
								sector->data[c] ^= sector->data[c-1];
							}
							if(sector->data.back()) continue;

							// Having checked the checksum, remove it.
							sector->data.resize(sector->data.size() - 1);

							if(is_five_and_three) {
								// TODO
							} else {
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

								// Throw away the collection of two-bit chunks from the start of the sector.
								sector->data.erase(sector->data.begin(), sector->data.end() - 256);

								// Add this sector to the map.
								sector->encoding = Sector::Encoding::SixAndTwo;
							}
							result.insert(std::make_pair(sector_location, std::move(*sector)));
						} break;

						case 704: {	// Potentially this is a Macintosh sector.
							// Attempt a six-and-two unmapping of the header.
							std::array<uint_fast8_t, 5> decoded_header;
							bool out_of_bounds = false;
							for(size_t c = 0; c < decoded_header.size(); ++c) {
								decoded_header[c] = unmap_six_and_two(header[c]);
								if(decoded_header[c] == 0xff) {
									out_of_bounds = true;
									break;
								}
							}
							if(out_of_bounds) {
								continue;
							}

							// Test the checksum.
							if(decoded_header[4] != (decoded_header[0] ^ decoded_header[1] ^ decoded_header[2] ^ decoded_header[3]))
								sector->has_header_checksum_error = true;

							// Decode the header.
							sector->address.track = uint8_t(decoded_header[0] | ((decoded_header[2]&0x1f) << 6));
							sector->address.sector = decoded_header[1];
							sector->address.format = decoded_header[3];
							sector->address.is_side_two = decoded_header[2] & 0x20;

							// Reverse the GCR encoding of the sector contents to get back to 6-bit data.
							for(auto &c: sector->data) {
								c = unmap_six_and_two(c);
								if(c == 0xff) {
									out_of_bounds = true;
									break;
								}
							}
							if(out_of_bounds) {
								continue;
							}

							// The first byte in the sector is a repeat of the sector number; test it
							// for correctness.
							if(sector->data[0] != sector->address.sector) {
								continue;
							}

							// Cf. the corresponding section of Encoder.cpp for logic below.
							int checksum[3] = {0, 0, 0};
							for(size_t c = 0; c < 175; ++c) {
								// Calculate the rolling checcksum in order to decode the bytes.
								checksum[0] = (checksum[0] << 1) | (checksum[0] >> 7);

								// All offsets are +1 below, to skip the initial sector number duplicate.
								const uint8_t top_bits = sector->data[1 + c*4];

								// Decode first byte.
								sector->data[0 + c * 3] = uint8_t((sector->data[2 + c*4] + ((top_bits & 0x30) << 2)) ^ checksum[0]);
								checksum[2] += sector->data[0 + c * 3] + (checksum[0] >> 8);

								// Decode second byte;
								sector->data[1 + c * 3] = uint8_t((sector->data[3 + c*4] + ((top_bits & 0x0c) << 4)) ^ checksum[2]);
								checksum[1] += sector->data[1 + c * 3] + (checksum[2] >> 8);

								// Decode third byte, if there is one.
								if(c != 174) {
									sector->data[2 + c * 3] = uint8_t((sector->data[4 + c*4] + ((top_bits & 0x03) << 6)) ^ checksum[1]);
									checksum[0] += sector->data[2 + c * 3] + (checksum[1] >> 8);
								}

								// Reset carries.
								checksum[0] &= 0xff;
								checksum[1] &= 0xff;
								checksum[2] &= 0xff;
							}

							// Test the checksum.
							if(
								checksum[0] != uint8_t(sector->data[703] + ((sector->data[700] & 0x03) << 6)) ||
								checksum[1] != uint8_t(sector->data[702] + ((sector->data[700] & 0x0c) << 4)) ||
								checksum[2] != uint8_t(sector->data[701] + ((sector->data[700] & 0x30) << 2))
							) sector->has_data_checksum_error = true;

							// Chop to size, and that's that.
							sector->data.resize(524);

							// Add this sector to the map.
							sector->encoding = Sector::Encoding::Macintosh;
							result.insert(std::make_pair(sector_location, std::move(*sector)));
						} break;
					}
				} else {
					new_sector->data.push_back(value);
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
