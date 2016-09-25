//
//  AcornAnalyser.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 29/08/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#include "StaticAnalyser.hpp"

#include "Disk.hpp"
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
	Target target;
	target.machine = Target::Electron;
	target.probability = 1.0; // TODO: a proper estimation
	target.acorn.has_dfs = false;
	target.acorn.has_adfs = false;

	// strip out inappropriate cartridges
	target.cartridges = AcornCartridgesFrom(cartridges);

	// if there are any tapes, attempt to get data from the first
	if(tapes.size() > 0)
	{
		std::shared_ptr<Storage::Tape::Tape> tape = tapes.front();
		tape->reset();
		std::list<File> files = GetFiles(tape);
		tape->reset();

		// continue if there are any files
		if(files.size())
		{
			bool is_basic = true;

			// protected files are always for *RUNning only
			if(files.front().is_protected) is_basic = false;

			// check also for a continuous threading of BASIC lines; if none then this probably isn't BASIC code,
			// so that's also justification to *RUN
			size_t pointer = 0;
			uint8_t *data = &files.front().data[0];
			size_t data_size = files.front().data.size();
			while(1)
			{
				if(pointer >= data_size-1 || data[pointer] != 13)
				{
					is_basic = false;
					break;
				}
				if((data[pointer+1]&0x7f) == 0x7f) break;
				pointer += data[pointer+3];
			}

			// Inspect first file. If it's protected or doesn't look like BASIC
			// then the loading command is *RUN. Otherwise it's CHAIN"".
			target.loadingCommand = is_basic ? "CHAIN\"\"\n" : "*RUN\n";

			target.tapes = tapes;
		}
	}

	if(disks.size() > 0)
	{
		std::shared_ptr<Storage::Disk::Disk> disk = disks.front();
		std::unique_ptr<Catalogue> catalogue = GetDFSCatalogue(disk);
		if(catalogue == nullptr) catalogue = GetADFSCatalogue(disk);
		if(catalogue)
		{
			target.disks = disks;
			target.acorn.has_dfs = true;

			switch(catalogue->bootOption)
			{
				case Catalogue::BootOption::None:	target.loadingCommand = "*CAT\n";		break;
				default:							target.acorn.should_hold_shift = true;	break;
			}
		}
	}

	if(target.tapes.size() || target.disks.size() || target.cartridges.size())
		destination.push_back(target);
}
