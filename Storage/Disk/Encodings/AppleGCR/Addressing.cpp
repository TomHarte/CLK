//
//  Addressing.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 26/11/2026.
//  Copyright © 2026 Thomas Harte. All rights reserved.
//

#include "Addressing.hpp"

using namespace Storage::Encodings::AppleGCR;

Address Storage::Encodings::AppleGCR::address_from_block_35(int block) {
	// Apple's 3.5" GCR is zoned; specifically:
	//
	//	tracks 0–15: 12 sectors/track;
	//	16–31: 11 sectors/track;
	//	32–47: 10 sectors/track;
	//	48–63: 9 sectors/track;
	//	64–80: 8 sectors/track.
	//
	// Blocks are numbered in asecnding order from track 0 upwards, and interleaved
	// between sides. So e.g. track 0 holds blocks 0–23 with 0–11 being on side 1 and
	// 12–23 being on side 2.

	const auto sectors_per_track = [](const int zone) {
		return 11 - (zone >> 4);
	};
	const auto sectors_per_zone = [&](const int zone) {
		return 2 * 16 * sectors_per_track(zone);
	};

	// Linear search for zone.
	int zone = 0;
	while(block > sectors_per_zone(zone)) {
		block -= sectors_per_zone(zone);
		++zone;
	}

	// Get track from there.
	const int track = (zone << 4) + block / (2 * sectors_per_track(zone));
	const int track_offset = block % (2 * sectors_per_track(zone));

	return {
		.side = track_offset / sectors_per_track(zone),
		.track = track,
		.sector = track_offset % sectors_per_track(zone)
	};
}
