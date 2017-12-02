//
//  StaticAnalyser.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 25/11/2017.
//  Copyright © 2017 Thomas Harte. All rights reserved.
//

#include "StaticAnalyser.hpp"

/*
DEFB "AB" ; expansion ROM header
DEFW initcode ; start of the init code, 0 if no initcode
DEFW callstat; pointer to CALL statement handler, 0 if no such handler
DEFW device; pointer to expansion device handler, 0 if no such handler
DEFW basic ; pointer to the start of a tokenized basicprogram, 0 if no basicprogram
DEFS 6,0 ; room reserved for future extensions
*/
static std::list<std::shared_ptr<Storage::Cartridge::Cartridge>>
		MSXCartridgesFrom(const std::list<std::shared_ptr<Storage::Cartridge::Cartridge>> &cartridges) {
	std::list<std::shared_ptr<Storage::Cartridge::Cartridge>> msx_cartridges;

	for(const auto &cartridge : cartridges) {
		const auto &segments = cartridge->get_segments();

		// Only one mapped item is allowed.
		if(segments.size() != 1) continue;

		// Which must be a multiple of 16 kb in size.
		Storage::Cartridge::Cartridge::Segment segment = segments.front();
		const size_t data_size = segment.data.size();
		if(data_size < 0x4000 || data_size & 0x3fff) continue;

		// Check for a ROM header at address 0; TODO: if it's not found then try 0x4000
		// and consider swapping the image.

		// Check for the expansion ROM header and the reserved bytes.
		if(segment.data[0] != 0x41 || segment.data[1] != 0x42) continue;
		bool all_zeroes = true;
		for(size_t c = 0; c < 6; ++c) {
			if(segment.data[10 + c] != 0) all_zeroes = false;
		}
		if(!all_zeroes) continue;

		// Pick a paging address based on the four pointers.
		uint16_t start_address = 0xc000;
		for(size_t c = 0; c < 8; c += 2) {
			uint16_t code_pointer = static_cast<uint16_t>(segment.data[2 + c] | segment.data[3 + c] << 8);
			if(code_pointer) {
				start_address = std::min(static_cast<uint16_t>(code_pointer &~ 0x3fff), start_address);
			}
		}

		// That'll do then, but apply the detected start address.
//		printf("%04x\n", start_address);

		msx_cartridges.emplace_back(new Storage::Cartridge::Cartridge({
			Storage::Cartridge::Cartridge::Segment(start_address, segment.data)
		}));
	}

	return msx_cartridges;
}

void StaticAnalyser::MSX::AddTargets(const Media &media, std::list<Target> &destination) {
	Target target;

	target.media.cartridges = MSXCartridgesFrom(media.cartridges);

	if(!target.media.empty()) {
		target.machine = Target::MSX;
		target.probability = 1.0;
		destination.push_back(target);
	}
}
