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

#include "../Disassembler/6502.hpp"
#include "../Disassembler/AddressMapper.hpp"

#include <algorithm>
#include <cstring>
#include <sstream>
#include <unordered_set>

using namespace Analyser::Static::Commodore;

namespace {

std::vector<std::shared_ptr<Storage::Cartridge::Cartridge>>
Vic20CartridgesFrom(const std::vector<std::shared_ptr<Storage::Cartridge::Cartridge>> &cartridges) {
	std::vector<std::shared_ptr<Storage::Cartridge::Cartridge>> vic20_cartridges;

	for(const auto &cartridge : cartridges) {
		const auto &segments = cartridge->get_segments();

		// Only one mapped item is allowed ...
		if(segments.size() != 1) continue;

		// ... which must be 16 kb in size.
		Storage::Cartridge::Cartridge::Segment segment = segments.front();
		if(segment.start_address != 0xa000) continue;
		if(!Storage::Cartridge::Encodings::CommodoreROM::isROM(segment.data)) continue;

		vic20_cartridges.push_back(cartridge);
	}

	// TODO: other machines?

	return vic20_cartridges;
}

struct BASICAnalysis {
	enum class Version {
		BASIC2,
		BASIC4,
		BASIC3_5,
	} minimum_version = Version::BASIC2;
	std::vector<uint16_t> machine_code_addresses;
};

std::optional<BASICAnalysis> analyse(const File &file) {
	// Accept only 'program' types.
	if(file.type != File::RelocatableProgram && file.type != File::NonRelocatableProgram) {
		return std::nullopt;
	}

	uint16_t line_address = file.starting_address;
	int previous_line_number = -1;

	const auto byte = [&](uint16_t address) {
		return file.data[address - file.starting_address];
	};
	const auto word = [&](uint16_t address) {
		return uint16_t(byte(address) | byte(address + 1) << 8);
	};

	// BASIC programs have a per-line structure of:
	//		[2 bytes: address of start of next line]
	//		[2 bytes: this line number]
	//		... null-terminated code ...
	//	(with a next line address of 0000 indicating end of program)
	//
	// If a SYS is encountered that jumps into the BASIC program then treat that as
	// a machine code entry point.

	BASICAnalysis analysis;
	std::unordered_set<uint16_t> visited_lines;
	while(true) {
		// Analysis has failed if there isn't at least one complete BASIC line from here.
		if(size_t(line_address - file.starting_address) + 5 >= file.data.size()) {
			return std::nullopt;
		}

		const auto next_line_address = word(line_address);
		const auto line_number = word(line_address + 2);

		uint16_t code = line_address + 4;
		const auto next = [&]() -> uint8_t {
			if(code >= file.starting_address + file.data.size()) {
				return 0;
			}
			return byte(code++);
		};

		while(true) {
			const auto token = next();
			if(!token || token == 0x8f) break;

			switch(token) {
				case 0x9e: {	// SYS; parse following ASCII argument.
					uint16_t address = 0;
					while(true) {
						const auto c = next();
						if(c < '0' || c > '9') {
							break;
						}
						address = (address * 10) + (c - '0');
					};
					analysis.machine_code_addresses.push_back(address);
				} break;
			}
		}

		// Exit if a formal end of the program has been declared or if, as some copy protections do,
		// the linked list of line contents has been made circular.
		visited_lines.insert(line_address);
		if(!next_line_address || visited_lines.find(next_line_address) != visited_lines.end()) {
			break;
		}

		previous_line_number = line_number;
		line_address = next_line_address;
	}

	return analysis;
}

}

Analyser::Static::TargetList Analyser::Static::Commodore::GetTargets(
	const Media &media,
	const std::string &file_name,
	TargetPlatform::IntType
) {
	TargetList destination;
	auto target = std::make_unique<Target>();

	int device = 0;
	std::vector<File> files;
	bool is_disk = false;

	// Strip out inappropriate cartridges.
	target->media.cartridges = Vic20CartridgesFrom(media.cartridges);

	// Find all valid Commodore files on disks.
	for(auto &disk : media.disks) {
		std::vector<File> disk_files = GetFiles(disk);
		if(!disk_files.empty()) {
			is_disk = true;
			files.insert(
				files.end(),
				std::make_move_iterator(disk_files.begin()),
				std::make_move_iterator(disk_files.end())
			);
			target->media.disks.push_back(disk);
			if(!device) device = 8;
		}
	}

	// Find all valid Commodore files on tapes.
	for(auto &tape : media.tapes) {
		std::vector<File> tape_files = GetFiles(tape);
		tape->reset();
		if(!tape_files.empty()) {
			files.insert(
				files.end(),
				std::make_move_iterator(tape_files.begin()),
				std::make_move_iterator(tape_files.end())
			);
			target->media.tapes.push_back(tape);
			if(!device) device = 1;
		}
	}

	// Inspect discovered files to try to divine machine and memory model.
	if(!files.empty()) {
		const auto &file = files.front();

		auto memory_model = Target::MemoryModel::Unexpanded;
		std::ostringstream string_stream;
		string_stream << "LOAD\"" << (is_disk ? "*" : "") << "\"," << device;

		const auto analysis = analyse(file);
		if(analysis && !analysis->machine_code_addresses.empty()) {
			string_stream << ",1";

			// Disassemble.
			const auto disassembly = Analyser::Static::MOS6502::Disassemble(
				file.data,
				Analyser::Static::Disassembler::OffsetMapper(file.starting_address),
				analysis->machine_code_addresses
			);

			// Very dumb check: if FF3E or FF3F were touched, this is for the +4.
			for(const auto address: {0xff3e, 0xff3f}) {
				for(const auto &collection: {
					disassembly.external_loads,
					disassembly.external_stores,
					disassembly.external_modifies
				}) {
					if(collection.find(uint16_t(address)) != collection.end()) {
						target->machine = Machine::Plus4;	// TODO: use a better target?
					}
				}
			}
		}

		string_stream << "\nRUN\n";
		target->loading_command = string_stream.str();

		// make a first guess based on loading address
		switch(files.front().starting_address) {
			default:
				Log::Logger<Log::Source::CommodoreStaticAnalyser>().error().append(
					"Unrecognised loading address for Commodore program: %04x", files.front().starting_address);
				[[fallthrough]];
			case 0x1001:
				memory_model = Target::MemoryModel::Unexpanded;
			break;
			case 0x1201:
				memory_model = Target::MemoryModel::ThirtyTwoKB;
			break;
			case 0x0401:
				memory_model = Target::MemoryModel::EightKB;
			break;

			case 0x1c01:
				Log::Logger<Log::Source::CommodoreStaticAnalyser>().info().append("Unimplemented: C128");
			break;
		}

		// The Vic-20 never has RAM after 0x8000.
		if(file.ending_address >= 0x8000) {
			target->machine = Machine::Plus4;
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
//				else if(target->memory_model == Target::MemoryModel::Unexpanded &&
//					!(starting_address >= 0x1000 || starting_address+file_size < 0x0400))
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
				//	M6:		this is a C64 file.
				//	MV:		this is a Vic-20 file.
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
