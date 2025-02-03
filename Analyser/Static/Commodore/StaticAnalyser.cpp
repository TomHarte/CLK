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
#include <optional>
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

	// Don't form an opinion if file is empty.
	if(file.data.empty()) {
		return std::nullopt;
	}

	uint16_t line_address = file.starting_address;
//	int previous_line_number = -1;

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
		if(size_t(line_address - file.starting_address) + 5 >= file.data.size() || line_address < file.starting_address) {
			analysis.machine_code_addresses.push_back(file.starting_address);
			break;
		}

		const auto next_line_address = word(line_address);
//		const auto line_number = word(line_address + 2);

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

//		previous_line_number = line_number;
		line_address = next_line_address;
	}

	return analysis;
}

template <typename TargetT>
void set_loading_command(TargetT &target) {
	if(target.media.disks.empty()) {
		target.loading_command = "LOAD\"\",1,1\nRUN\n";
	} else {
		target.loading_command = "LOAD\"*\",8,1\nRUN\n";
	}
}

bool obviously_uses_ted(const File &file) {
	const auto analysis = analyse(file);
	if(!analysis) return false;

	// Disassemble.
	const auto disassembly = Analyser::Static::MOS6502::Disassemble(
		file.data,
		Analyser::Static::Disassembler::OffsetMapper(file.starting_address),
		analysis->machine_code_addresses
	);

	// If FF3E or FF3F is touched, this is for the +4.
	// TODO: probably require a very early touch.
	for(const auto address: {0xff3e, 0xff3f}) {
		for(const auto &collection: {
			disassembly.external_loads,
			disassembly.external_stores,
			disassembly.external_modifies
		}) {
			if(collection.find(uint16_t(address)) != collection.end()) {
				return true;
			}
		}
	}

	return false;
}

struct FileAnalysis {
	int device = 0;
	std::vector<File> files;
	bool is_disk = false;
	Analyser::Static::Media media;
};

template <TargetPlatform::Type platform>
FileAnalysis analyse_files(const Analyser::Static::Media &media) {
	FileAnalysis analysis;

	// Find all valid Commodore files on disks.
	for(auto &disk : media.disks) {
		std::vector<File> disk_files = GetFiles(disk);
		if(!disk_files.empty()) {
			analysis.is_disk = true;
			analysis.files.insert(
				analysis.files.end(),
				std::make_move_iterator(disk_files.begin()),
				std::make_move_iterator(disk_files.end())
			);
			analysis.media.disks.push_back(disk);
			if(!analysis.device) analysis.device = 8;
		}
	}

	// Find all valid Commodore files on tapes.
	for(auto &tape : media.tapes) {
		auto serialiser = tape->serialiser();
		std::vector<File> tape_files = GetFiles(*serialiser, platform);
		if(!tape_files.empty()) {
			analysis.files.insert(
				analysis.files.end(),
				std::make_move_iterator(tape_files.begin()),
				std::make_move_iterator(tape_files.end())
			);
			analysis.media.tapes.push_back(tape);
			if(!analysis.device) analysis.device = 1;
		}
	}

	return analysis;
}

std::string loading_command(const FileAnalysis &file_analysis) {
	std::ostringstream string_stream;
	string_stream << "LOAD\"" << (file_analysis.is_disk ? "*" : "") << "\"," << file_analysis.device;

	const auto analysis = analyse(file_analysis.files[0]);
	if(analysis && !analysis->machine_code_addresses.empty()) {
		string_stream << ",1";
	}
	string_stream << "\nRUN\n";
	return string_stream.str();
}

std::pair<TargetPlatform::IntType, std::optional<Vic20Target::MemoryModel>>
analyse_starting_address(uint16_t starting_address) {
	switch(starting_address) {
		case 0x1c01:
			// TODO: assume C128.
		default:
			Log::Logger<Log::Source::CommodoreStaticAnalyser>().error().append(
				"Unrecognised loading address for Commodore program: %04x", starting_address);
			[[fallthrough]];
		case 0x1001:
		return std::make_pair(TargetPlatform::Vic20 | TargetPlatform::Plus4, Vic20Target::MemoryModel::Unexpanded);

		case 0x1201:	return std::make_pair(TargetPlatform::Vic20, Vic20Target::MemoryModel::ThirtyTwoKB);
		case 0x0401:	return std::make_pair(TargetPlatform::Vic20, Vic20Target::MemoryModel::EightKB);
		case 0x0801:	return std::make_pair(TargetPlatform::C64, std::nullopt);
	}
}

template <TargetPlatform::IntType platform>
std::unique_ptr<Analyser::Static::Target> get_target(
	const Analyser::Static::Media &media,
	const std::string &file_name,
	bool is_confident
);

template<>
std::unique_ptr<Analyser::Static::Target> get_target<TargetPlatform::Plus4>(
	const Analyser::Static::Media &media,
	const std::string &,
	bool is_confident
) {
	auto target = std::make_unique<Plus4Target>();
	if(is_confident) {
		target->media = media;
		set_loading_command(*target);
	} else {
		const auto files = analyse_files<TargetPlatform::Plus4>(media);
		if(!files.files.empty()) {
			target->loading_command = loading_command(files);
		}
		target->media.disks = media.disks;
		target->media.tapes = media.tapes;
	}

	// Attach a 1541 if there are any disks here.
	target->has_c1541 = !target->media.disks.empty();
	return std::move(target);
}

template<>
std::unique_ptr<Analyser::Static::Target> get_target<TargetPlatform::Vic20>(
	const Analyser::Static::Media &media,
	const std::string &file_name,
	bool is_confident
) {
	auto target = std::make_unique<Vic20Target>();
	const auto files = analyse_files<TargetPlatform::Vic20>(media);
	if(!files.files.empty()) {
		target->loading_command = loading_command(files);

		const auto model = analyse_starting_address(files.files[0].starting_address);
		if(model.second.has_value()) {
			target->set_memory_model(*model.second);
		}
	}

	if(is_confident) {
		target->media = media;
		set_loading_command(*target);
	} else {
		// Strip out inappropriate cartridges but retain all tapes and disks.
		target->media.cartridges = Vic20CartridgesFrom(media.cartridges);
		target->media.disks = media.disks;
		target->media.tapes = media.tapes;
	}

	for(const auto &file : files.files) {
		// The Vic-20 never has RAM after 0x8000.
		if(file.ending_address >= 0x8000) {
			return nullptr;
		}

		if(obviously_uses_ted(file)) {
			return nullptr;
		}
	}

	// Inspect filename for configuration hints.
	if(!target->media.empty()) {
		using Region = Analyser::Static::Commodore::Vic20Target::Region;

		std::string lowercase_name = file_name;
		std::transform(lowercase_name.begin(), lowercase_name.end(), lowercase_name.begin(), ::tolower);

		// Hint 1: 'ntsc' anywhere in the name implies America.
		if(lowercase_name.find("ntsc") != std::string::npos) {
			target->region = Region::American;
		}

		// Potential additional hints: check for TheC64 tags; these are Vic-20 exclusive.
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
					target->region = Region::American;
				}
				if(!strcmp(next_tag, "tp")) {	// i.e. PAL.
					target->region = Region::European;
				}

				// Unhandled:
				//
				//	M6:		this is a C64 file.
				//	MV:		this is a Vic-20 file.
				//	J1/J2:	this C64 file should have the primary joystick in slot 1/2.
				//	RO:		this disk image should be treated as read-only.
			}
		}
	}

	// Attach a 1540 if there are any disks here.
	target->has_c1540 = !target->media.disks.empty();
	return std::move(target);
}

}


Analyser::Static::TargetList Analyser::Static::Commodore::GetTargets(
	const Media &media,
	const std::string &file_name,
	TargetPlatform::IntType platforms,
	bool is_confident
) {
	TargetList destination;

	if(platforms & TargetPlatform::Vic20) {
		auto vic20 = get_target<TargetPlatform::Vic20>(media, file_name, is_confident);
		if(vic20) {
			destination.push_back(std::move(vic20));
		}
	}

	if(platforms & TargetPlatform::Plus4) {
		auto plus4 = get_target<TargetPlatform::Plus4>(media, file_name, is_confident);
		if(plus4) {
			destination.push_back(std::move(plus4));
		}
	}

	return destination;
}
