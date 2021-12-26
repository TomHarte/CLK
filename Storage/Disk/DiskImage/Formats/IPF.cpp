//
//  IPF.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 25/12/2021.
//  Copyright © 2021 Thomas Harte. All rights reserved.
//

#include "IPF.hpp"

using namespace Storage::Disk;

namespace {

constexpr uint32_t block(const char (& src)[5]) {
	return uint32_t(
		(uint32_t(src[0]) << 24) |
		(uint32_t(src[1]) << 16) |
		(uint32_t(src[2]) << 8) |
		uint32_t(src[3])
	);
}

}


IPF::IPF(const std::string &file_name) : file_(file_name) {
	while(true) {
		const auto start_of_block = file_.tell();
		const uint32_t type = file_.get32be();
		uint32_t length = file_.get32be();						// Can't be const because of the dumb encoding of DATA blocks.
		[[maybe_unused]] const uint32_t crc = file_.get32be();
		if(file_.eof()) break;

		// Sanity check: the first thing in a file should be the CAPS record.
		if(!start_of_block && type != block("CAPS")) {
			throw Error::InvalidFormat;
		}

		switch(type) {
			default:
				printf("Ignoring %c%c%c%c, starting at %ld of length %d\n", (type >> 24), (type >> 16) & 0xff, (type >> 8) & 0xff, type & 0xff, start_of_block, length);
			break;

			case block("CAPS"):
				// Analogously to the sanity check above, if a CAPS block is anywhere other
				// than first then something is amiss.
				if(start_of_block) {
					throw Error::InvalidFormat;
				}
			break;

			case block("INFO"): {
				// There are a lot of useful archival fields in the info chunk, which for emulation
				// aren't that interesting.

				// Make sure this is a floppy disk.
				const uint32_t media_type = file_.get32be();
				if(media_type != 1) {
					throw Error::InvalidFormat;
				}

				// Skip: encoder type, revision, file key and revision, CRC of the original .ctr, and minimum track.
				file_.seek(24, SEEK_CUR);
				track_count_ = int(1 + file_.get32be());

				// Skip: min side.
				file_.seek(4, SEEK_CUR);
				head_count_ = int(1 + file_.get32be());

				// Skip: creation date, time.
				file_.seek(8, SEEK_CUR);

				platform_type_ = 0;
				for(int c = 0; c < 4; c++) {
					const uint8_t platform = file_.get8();
					switch(platform) {
						default: break;
						case 1:	platform_type_ |= TargetPlatform::Amiga;		break;
						case 2:	platform_type_ |= TargetPlatform::AtariST;		break;
						/* Omitted: 3 -> IBM PC */
						case 4:	platform_type_ |= TargetPlatform::AmstradCPC;	break;
						case 5:	platform_type_ |= TargetPlatform::ZXSpectrum;	break;
						/* Omitted: 6 -> Sam Coupé */
						/* Omitted: 7 -> Archimedes */
						/* Omitted: 8 -> C64 */
						/* Omitted: 9 -> Atari 8-bit */
					}
				}

				// If the file didn't declare anything, default to supporting everything.
				if(!platform_type_) {
					platform_type_ = ~0;
				}

				// Ignore: disk number, creator ID, reserved area.
			} break;

			case block("IMGE"):
			break;

			case block("DATA"): {
				length += file_.get32be();
				printf("Handling DATA block at %ld of length %d\n", start_of_block, length);
			} break;
		}

		file_.seek(start_of_block + length, SEEK_SET);
	}
}

HeadPosition IPF::get_maximum_head_position() {
	return HeadPosition(track_count_);
}

int IPF::get_head_count() {
	return head_count_;
}

std::shared_ptr<Track> IPF::get_track_at_position([[maybe_unused]] Track::Address address) {
	return nullptr;
}
