//
//  StaticAnalyser.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 25/11/2017.
//  Copyright © 2017 Thomas Harte. All rights reserved.
//

#include "StaticAnalyser.hpp"

#include "Tape.hpp"

/*
	Expected standard cartridge format:

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
		if(data_size < 0x2000 || data_size & 0x3fff) continue;

		// Check for a ROM header at address 0; TODO: if it's not found then try 0x4000
		// and consider swapping the image.

		// Check for the expansion ROM header and the reserved bytes.
		if(segment.data[0] != 0x41 || segment.data[1] != 0x42) continue;

		// Apply the standard MSX start address.
		msx_cartridges.emplace_back(new Storage::Cartridge::Cartridge({
			Storage::Cartridge::Cartridge::Segment(0x4000, segment.data)
		}));
	}

	return msx_cartridges;
}

void StaticAnalyser::MSX::AddTargets(const Media &media, std::list<Target> &destination) {
	Target target;

	// Obtain only those cartridges which it looks like an MSX would understand.
	target.media.cartridges = MSXCartridgesFrom(media.cartridges);

	// Check tapes for loadable files.
	for(const auto &tape : media.tapes) {
		std::vector<File> files_on_tape = GetFiles(tape);
		if(!files_on_tape.empty()) {
			switch(files_on_tape.front().type) {
				case File::Type::ASCII:				target.loadingCommand = "RUN\"CAS:\n";			break;
				case File::Type::TokenisedBASIC:	target.loadingCommand = "CLOAD\nRUN\n";			break;
				case File::Type::Binary:			target.loadingCommand = "BLOAD\"CAS:\",R\n";	break;
				default: break;
			}
			target.media.tapes.push_back(tape);
		}
	}

	if(!target.media.empty()) {
		target.machine = Target::MSX;
		target.probability = 1.0;
		destination.push_back(target);
	}
}
