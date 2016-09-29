//
//  CommodoreAnalyser.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 06/09/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#include "StaticAnalyser.hpp"

#include "File.hpp"
#include "Tape.hpp"
#include "Disk.hpp"
#include "../../Storage/Cartridge/Encodings/CommodoreROM.hpp"

using namespace StaticAnalyser::Commodore;

static std::list<std::shared_ptr<Storage::Cartridge::Cartridge>>
	Vic20CartridgesFrom(const std::list<std::shared_ptr<Storage::Cartridge::Cartridge>> &cartridges)
{
	std::list<std::shared_ptr<Storage::Cartridge::Cartridge>> vic20_cartridges;

	for(std::shared_ptr<Storage::Cartridge::Cartridge> cartridge : cartridges)
	{
		const std::list<Storage::Cartridge::Cartridge::Segment> &segments = cartridge->get_segments();

		// only one mapped item is allowed
		if(segments.size() != 1) continue;

		// which must be 16 kb in size
		Storage::Cartridge::Cartridge::Segment segment = segments.front();
		if(segment.start_address != 0xa000) continue;
		if(!Storage::Cartridge::Encodings::CommodoreROM::isROM(segment.data)) continue;

		vic20_cartridges.push_back(cartridge);
	}

	return vic20_cartridges;
}

void StaticAnalyser::Commodore::AddTargets(
	const std::list<std::shared_ptr<Storage::Disk::Disk>> &disks,
	const std::list<std::shared_ptr<Storage::Tape::Tape>> &tapes,
	const std::list<std::shared_ptr<Storage::Cartridge::Cartridge>> &cartridges,
	std::list<StaticAnalyser::Target> &destination)
{
	Target target;
	target.machine = Target::Vic20;	// TODO: machine estimation
	target.probability = 1.0; // TODO: a proper estimation

	int device = 0;
	std::list<File> files;
	bool is_disk = false;

	// strip out inappropriate cartridges
	target.cartridges = Vic20CartridgesFrom(cartridges);

	// check disks
	for(auto &disk : disks)
	{
		std::list<File> disk_files = GetFiles(disk);
		if(disk_files.size())
		{
			is_disk = true;
			files.splice(files.end(), disk_files);
			target.disks = disks;
			if(!device) device = 8;
		}
	}

	// check tapes
	for(auto &tape : tapes)
	{
		std::list<File> tape_files = GetFiles(tape);
		if(tape_files.size())
		{
			files.splice(files.end(), tape_files);
			target.tapes = tapes;
			if(!device) device = 1;
		}
	}

	if(files.size())
	{
		target.vic20.memory_model = Vic20MemoryModel::Unexpanded;
		if(files.front().is_basic())
		{
			char command[16];
			snprintf(command, 16, "LOAD\"%s\",%d,0\nRUN\n", is_disk ? "*" : "", device);
			target.loadingCommand = command;
		}
		else
		{
			char command[16];
			snprintf(command, 16, "LOAD\"%s\",%d,1\nRUN\n", is_disk ? "*" : "", device);
			target.loadingCommand = command;
		}

		// make a first guess based on loading address
		switch(files.front().starting_address)
		{
			case 0x1001:
			default: break;
			case 0x1201:
				target.vic20.memory_model = Vic20MemoryModel::ThirtyTwoKB;
			break;
			case 0x0401:
				target.vic20.memory_model = Vic20MemoryModel::EightKB;
			break;
		}

		// General approach: increase memory size conservatively such that the largest file found will fit.
		for(File &file : files)
		{
			size_t file_size = file.data.size();
//			bool is_basic = file.is_basic();

			/*if(is_basic)
			{
				// BASIC files may be relocated, so the only limit is size.
				//
				// An unexpanded machine has 3583 bytes free for BASIC;
				// a 3kb expanded machine has 6655 bytes free.
				if(file_size > 6655)
					target.vic20.memory_model = Vic20MemoryModel::ThirtyTwoKB;
				else if(target.vic20.memory_model == Vic20MemoryModel::Unexpanded && file_size > 3583)
					target.vic20.memory_model = Vic20MemoryModel::EightKB;
			}
			else
			{*/
//			if(!file.type == File::NonRelocatableProgram)
//			{
				// Non-BASIC files may be relocatable but, if so, by what logic?
				// Given that this is unknown, take starting address as literal
				// and check against memory windows.
				//
				// (ignoring colour memory...)
				// An unexpanded Vic has memory between 0x0000 and 0x0400; and between 0x1000 and 0x2000.
				// A 3kb expanded Vic fills in the gap and has memory between 0x0000 and 0x2000.
				// A 32kb expanded Vic has memory in the entire low 32kb.
				uint16_t starting_address = file.starting_address;

				// If anything above the 8kb mark is touched, mark as a 32kb machine; otherwise if the
				// region 0x0400 to 0x1000 is touched and this is an unexpanded machine, mark as 3kb.
				if(starting_address + file_size > 0x2000)
					target.vic20.memory_model = Vic20MemoryModel::ThirtyTwoKB;
				else if(target.vic20.memory_model == Vic20MemoryModel::Unexpanded && !(starting_address >= 0x1000 || starting_address+file_size < 0x0400))
					target.vic20.memory_model = Vic20MemoryModel::ThirtyTwoKB;
//			}
		}
	}

	if(target.tapes.size() || target.cartridges.size() || target.disks.size())
		destination.push_back(target);
}
