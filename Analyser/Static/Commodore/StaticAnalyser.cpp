//
//  CommodoreAnalyser.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 06/09/2016.
//  Copyright 2016 Thomas Harte. All rights reserved.
//

#include "StaticAnalyser.hpp"

#include "Disk.hpp"
#include "File.hpp"
#include "Tape.hpp"
#include "Target.hpp"
#include "../../../Storage/Cartridge/Encodings/CommodoreROM.hpp"
#include "../../../Outputs/Log.hpp"

#include <algorithm>
#include <sstream>

using namespace Analyser::Static::Commodore;

static std::vector<std::shared_ptr<Storage::Cartridge::Cartridge>>
		Vic20CartridgesFrom(const std::vector<std::shared_ptr<Storage::Cartridge::Cartridge>> &cartridges) {
	std::vector<std::shared_ptr<Storage::Cartridge::Cartridge>> vic20_cartridges;

	for(const auto &cartridge : cartridges) {
		const auto &segments = cartridge->get_segments();

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

Analyser::Static::TargetList Analyser::Static::Commodore::GetTargets(const Media &media, const std::string &file_name, TargetPlatform::IntType potential_platforms) {
	TargetList destination;

	auto target = std::make_unique<Target>();
	target->machine = Machine::Vic20;	// TODO: machine estimation
	target->confidence = 0.5; // TODO: a proper estimation

	int device = 0;
	std::vector<File> files;
	bool is_disk = false;

	// strip out inappropriate cartridges
	target->media.cartridges = Vic20CartridgesFrom(media.cartridges);

	// check disks
	for(auto &disk : media.disks) {
		std::vector<File> disk_files = GetFiles(disk);
		if(!disk_files.empty()) {
			is_disk = true;
			files.insert(files.end(), disk_files.begin(), disk_files.end());
			target->media.disks.push_back(disk);
			if(!device) device = 8;
		}
	}

	// check tapes
	for(auto &tape : media.tapes) {
		std::vector<File> tape_files = GetFiles(tape);
		tape->reset();
		if(!tape_files.empty()) {
			files.insert(files.end(), tape_files.begin(), tape_files.end());
			target->media.tapes.push_back(tape);
			if(!device) device = 1;
		}
	}

	if(!files.empty()) {
		auto memory_model = Target::MemoryModel::Unexpanded;
		std::ostringstream string_stream;
		string_stream << "LOAD\"" << (is_disk ? "*" : "") << "\"," << device << ",";
		if(files.front().is_basic()) {
			string_stream << "0";
		} else {
			string_stream << "1";
		}
		string_stream << "\nRUN\n";
		target->loading_command = string_stream.str();

		// make a first guess based on loading address
		switch(files.front().starting_address) {
			default:
				LOG("Unrecognised loading address for Commodore program: " << PADHEX(4) <<  files.front().starting_address);
			case 0x1001:
				memory_model = Target::MemoryModel::Unexpanded;
			break;
			case 0x1201:
				memory_model = Target::MemoryModel::ThirtyTwoKB;
			break;
			case 0x0401:
				memory_model = Target::MemoryModel::EightKB;
			break;
		}

		target->set_memory_model(memory_model);

		// General approach: increase memory size conservatively such that the largest file found will fit.
//		for(File &file : files) {
//			std::size_t file_size = file.data.size();
//			bool is_basic = file.is_basic();

			/*if(is_basic)
			{
				// BASIC files may be relocated, so the only limit is size.
				//
				// An unexpanded machine has 3583 bytes free for BASIC;
				// a 3kb expanded machine has 6655 bytes free.
				if(file_size > 6655)
					target->vic20.memory_model = Vic20MemoryModel::ThirtyTwoKB;
				else if(target->vic20.memory_model == Vic20MemoryModel::Unexpanded && file_size > 3583)
					target->vic20.memory_model = Vic20MemoryModel::EightKB;
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
//				uint16_t starting_address = file.starting_address;

				// If anything above the 8kb mark is touched, mark as a 32kb machine; otherwise if the
				// region 0x0400 to 0x1000 is touched and this is an unexpanded machine, mark as 3kb.
//				if(starting_address + file_size > 0x2000)
//					target->memory_model = Target::MemoryModel::ThirtyTwoKB;
//				else if(target->memory_model == Target::MemoryModel::Unexpanded && !(starting_address >= 0x1000 || starting_address+file_size < 0x0400))
//					target->memory_model = Target::MemoryModel::ThirtyTwoKB;
//			}
//		}
	}

	if(!target->media.empty()) {
		// Inspect filename for configuration hints.
		std::string lowercase_name = file_name;
		std::transform(lowercase_name.begin(), lowercase_name.end(), lowercase_name.begin(), ::tolower);

		// Hint 1: 'ntsc' anywhere in the name implies America.
		if(lowercase_name.find("ntsc") != std::string::npos) {
			target->region = Analyser::Static::Commodore::Target::Region::American;
		}

		// Potential additional hints: check for TheC64 tags.
		auto final_underscore = lowercase_name.find_last_of('_');
		if(final_underscore != std::string::npos) {
			auto iterator = lowercase_name.begin() + ssize_t(final_underscore) + 1;

			while(iterator != lowercase_name.end()) {
				// Grab the next tag.
				char next_tag[3] = {0, 0, 0};
				next_tag[0] = *iterator++;
				if(iterator == lowercase_name.end()) break;
				next_tag[1] = *iterator++;

				// Exit early if attempting to read another tag has run over the file extension.
				if(next_tag[0] == '.' || next_tag[1] == '.') break;

				// Check whether it's anything.
				target->enabled_ram.bank0 |= !strcmp(next_tag, "b0");
				target->enabled_ram.bank1 |= !strcmp(next_tag, "b1");
				target->enabled_ram.bank2 |= !strcmp(next_tag, "b2");
				target->enabled_ram.bank3 |= !strcmp(next_tag, "b3");
				target->enabled_ram.bank5 |= !strcmp(next_tag, "b5");
				if(!strcmp(next_tag, "tn")) {	// i.e. NTSC.
					target->region = Analyser::Static::Commodore::Target::Region::American;
				}
				if(!strcmp(next_tag, "tp")) {	// i.e. PAL.
					target->region = Analyser::Static::Commodore::Target::Region::European;
				}

				// Unhandled:
				//
				//	M6: 	this is a C64 file.
				//	MV: 	this is a Vic-20 file.
				//	J1/J2:	this C64 file should have the primary joystick in slot 1/2.
				//	RO:		this disk image should be treated as read-only.
			}
		}

		// Attach a 1540 if there are any disks here.
		target->has_c1540 = !target->media.disks.empty();

		destination.push_back(std::move(target));
	}

	return destination;
}
