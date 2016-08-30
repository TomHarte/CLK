//
//  AcornAnalyser.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 29/08/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#include "AcornAnalyser.hpp"

#include "Tape.hpp"

using namespace StaticAnalyser::Acorn;

static std::list<std::shared_ptr<Storage::Cartridge::Cartridge>>
	AcornCartridgesFrom(const std::list<std::shared_ptr<Storage::Cartridge::Cartridge>> &cartridges)
{
	std::list<std::shared_ptr<Storage::Cartridge::Cartridge>> acorn_cartridges;

	for(std::shared_ptr<Storage::Cartridge::Cartridge> cartridge : cartridges)
	{
		const std::list<Storage::Cartridge::Cartridge::Segment> &segments = cartridge->get_segments();

		// only one mapped item is allowed
		if(segments.size() != 1) continue;

		// which must be 16 kb in size
		Storage::Cartridge::Cartridge::Segment segment = segments.front();
		if(segment.data.size() != 0x4000) continue;

		// is a copyright string present?
		uint8_t copyright_offset = segment.data[7];
		if(
			segment.data[copyright_offset] != 0x00 ||
			segment.data[copyright_offset+1] != 0x28 ||
			segment.data[copyright_offset+2] != 0x43 ||
			segment.data[copyright_offset+3] != 0x29
		) continue;

		// is the language entry point valid?
		if(!(
			(segment.data[0] == 0x00 && segment.data[1] == 0x00 && segment.data[2] == 0x00) ||
			(segment.data[0] != 0x00 && segment.data[2] >= 0x80 && segment.data[2] < 0xc0)
			)) continue;

		// is the service entry point valid?
		if(!(segment.data[5] >= 0x80 && segment.data[5] < 0xc0)) continue;

		// probability of a random binary blob that isn't an Acorn ROM proceeding to here:
		//		1/(2^32) *
		//		( ((2^24)-1)/(2^24)*(1/4)		+		1/(2^24)	) *
		//		1/4
		//	= something very improbable — around 1/16th of 1 in 2^32, but not exactly.
		acorn_cartridges.push_back(cartridge);
	}

	return acorn_cartridges;
}

void StaticAnalyser::Acorn::AddTargets(
	const std::list<std::shared_ptr<Storage::Disk::Disk>> &disks,
	const std::list<std::shared_ptr<Storage::Tape::Tape>> &tapes,
	const std::list<std::shared_ptr<Storage::Cartridge::Cartridge>> &cartridges,
	std::list<StaticAnalyser::Target> &destination)
{
	// strip out inappropriate cartridges
	std::list<std::shared_ptr<Storage::Cartridge::Cartridge>> acornCartridges = AcornCartridgesFrom(cartridges);

	// if there are any tapes, attempt to get data from the first
	if(tapes.size() > 0)
	{
		std::shared_ptr<Storage::Tape::Tape> tape = tapes.front();
		tape->reset();
		GetNextFile(tape);
	}
}
