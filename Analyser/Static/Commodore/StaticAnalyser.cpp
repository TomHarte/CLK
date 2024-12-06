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
		NotBASIC,
		BASIC2,
		BASIC4,
		BASIC3_5,
	} minimum_version = Version::NotBASIC;
	std::vector<uint16_t> machine_code_addresses;
};

std::optional<BASICAnalysis> analyse(const File &file) {
	BASICAnalysis analysis;

	switch(file.type) {
		// For 'program' types, proceed with analysis below.
		case File::RelocatableProgram:
		case File::NonRelocatableProgram:
		break;

		// For sequential and relative data stop right now.
		case File::DataSequence:
		case File::Relative:
		return std::nullopt;

		// For user data, try decoding from the starting point.
		case File::User:
			analysis.machine_code_addresses.push_back(file.starting_address);
		return analysis;
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

	std::unordered_set<uint16_t> visited_lines;
	while(true) {
		// Analysis has failed if there isn't at least one complete BASIC line from here.
		// Fall back on guessing the start address as a machine code entrypoint.
		if(size_t(line_address - file.starting_address) + 5 >= file.data.size()) {
			analysis.machine_code_addresses.push_back(file.starting_address);
			break;
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

		// TODO: sanity check on apparent line contents.
		// TODO: observe token set (and possibly parameters?) to guess BASIC version.
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
	auto vic_memory_model = Target::MemoryModel::Unexpanded;

	if(files.size() > 1) {
		printf("");
	}

	auto it = files.begin();
	while(it != files.end()) {
		const auto &file = *it;

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
		if(it == files.begin()) {
			target->loading_command = string_stream.str();
		}

		// make a first guess based on loading address
		switch(files.front().starting_address) {
			default:
				Log::Logger<Log::Source::CommodoreStaticAnalyser>().error().append(
					"Unrecognised loading address for Commodore program: %04x", files.front().starting_address);
				[[fallthrough]];
			case 0x1001:
				vic_memory_model = Target::MemoryModel::Unexpanded;
			break;
			case 0x1201:
				vic_memory_model = Target::MemoryModel::ThirtyTwoKB;
			break;
			case 0x0401:
				vic_memory_model = Target::MemoryModel::EightKB;
			break;

			case 0x1c01:
				Log::Logger<Log::Source::CommodoreStaticAnalyser>().info().append("Unimplemented: C128");
			break;
		}

		// The Vic-20 never has RAM after 0x8000.
		if(file.ending_address >= 0x8000) {
			target->machine = Machine::Plus4;
		}

		target->set_memory_model(vic_memory_model);

		++it;
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
