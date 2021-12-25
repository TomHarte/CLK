//
//  IPF.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 25/12/2021.
//  Copyright © 2021 Thomas Harte. All rights reserved.
//

#include "IPF.hpp"

using namespace Storage::Disk;


IPF::IPF(const std::string &file_name) : file_(file_name) {
	while(true) {
		const auto start_of_block = file_.tell();
		const uint32_t type = file_.get32be();
		uint32_t length = file_.get32be();						// Can't be const because of the dumb encoding of DATA blocks.
		[[maybe_unused]] const uint32_t crc = file_.get32be();
		if(file_.eof()) break;

#define BLOCK(a, b, c, d) ((a << 24) | (b << 16) | (c << 8) | d)

		// Sanity check: the first thing in a file should be the CAPS record.
		if(!start_of_block && type != BLOCK('C', 'A', 'P', 'S')) {
			throw Error::InvalidFormat;
		}

		switch(type) {
			default:
				printf("Ignoring %c%c%c%c, starting at %ld of length %d\n", (type >> 24), (type >> 16) & 0xff, (type >> 8) & 0xff, type & 0xff, start_of_block, length);
			break;

			case BLOCK('C', 'A', 'P', 'S'):
				// Analogously to the sanity check above, if a CAPS block is anywhere other
				// than first then something is amiss.
				if(start_of_block) {
					throw Error::InvalidFormat;
				}
			break;

			case BLOCK('D', 'A', 'T', 'A'): {
				length += file_.get32be();
				printf("Handling DATA block at %ld of length %d\n", start_of_block, length);
			} break;
		}
#undef BLOCK

		file_.seek(start_of_block + length, SEEK_SET);
	}
}

HeadPosition IPF::get_maximum_head_position() {
	return HeadPosition(80); // TODO;
}

int IPF::get_head_count() {
	return 2; // TODO;
}

std::shared_ptr<Track> IPF::get_track_at_position([[maybe_unused]] Track::Address address) {
	return nullptr;
}
