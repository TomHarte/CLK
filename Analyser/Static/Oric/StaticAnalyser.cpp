//
//  StaticAnalyser.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 11/10/2016.
//  Copyright 2016 Thomas Harte. All rights reserved.
//

#include "StaticAnalyser.hpp"

#include "Tape.hpp"
#include "Target.hpp"

#include "../Disassembler/6502.hpp"
#include "../Disassembler/AddressMapper.hpp"

#include "../../../Storage/Disk/Encodings/MFM/Parser.hpp"

#include <cstring>

using namespace Analyser::Static::Oric;

namespace {

int score(const Analyser::Static::MOS6502::Disassembly &disassembly, const std::set<uint16_t> &rom_functions, const std::set<uint16_t> &variable_locations) {
	int score = 0;

	for(const auto address : disassembly.outward_calls)		score += (rom_functions.find(address) != rom_functions.end()) ? 1 : -1;
	for(const auto address : disassembly.external_stores)	score += (variable_locations.find(address) != variable_locations.end()) ? 1 : -1;
	for(const auto address : disassembly.external_loads)	score += (variable_locations.find(address) != variable_locations.end()) ? 1 : -1;

	return score;
}

int basic10_score(const Analyser::Static::MOS6502::Disassembly &disassembly) {
	const std::set<uint16_t> rom_functions = {
		0x0228,	0x022b,
		0xc3ca,	0xc3f8,	0xc448,	0xc47c,	0xc4b5,	0xc4e3,	0xc4e0,	0xc524,	0xc56f,	0xc5a2,	0xc5f8,	0xc60a,	0xc6a5,	0xc6de,	0xc719,	0xc738,
		0xc773,	0xc824,	0xc832,	0xc841,	0xc8c1,	0xc8fe,	0xc91f,	0xc93f,	0xc941,	0xc91e,	0xc98b,	0xc996,	0xc9b3,	0xc9e0,	0xca0a,	0xca1c,
		0xca1f,	0xca3e,	0xca61,	0xca78,	0xca98,	0xcad2,	0xcb61,	0xcb9f,	0xcc59,	0xcbed,	0xcc0a,	0xcc8c,	0xcc8f,	0xccba,	0xccc9,	0xccfd,
		0xce0c,	0xce77,	0xce8b,	0xcfac,	0xcf74,	0xd03c,	0xd059,	0xcff0,	0xd087,	0xd0f2,	0xd0fc,	0xd361,	0xd3eb,	0xd47e,	0xd4a6,	0xd401,
		0xd593,	0xd5a3,	0xd4fa,	0xd595,	0xd730,	0xd767,	0xd816,	0xd82a,	0xd856,	0xd861,	0xd8a6,	0xd8b5,	0xd80a,	0xd867,	0xd938,	0xd894,
		0xd89d,	0xd8ac,	0xd983,	0xd993,	0xd9b5,	0xd93d,	0xd965,	0xda3f,	0xd9c6,	0xda16,	0xdaab,	0xdada,	0xda6b,	0xdb92,	0xdbb9,	0xdc79,
		0xdd4d,	0xdda3,	0xddbf,	0xd0d0,	0xde77,	0xdef4,	0xdf0b,	0xdf0f,	0xdf04,	0xdf12,	0xdf31,	0xdf4c,	0xdf8c,	0xdfa5,	0xdfcf,	0xe076,
		0xe0c1,	0xe22a,	0xe27c,	0xe2a6,	0xe313,	0xe34b,	0xe387,	0xe38e,	0xe3d7,	0xe407,	0xe43b,	0xe46f,	0xe4a8,	0xe4f2,	0xe554,	0xe57d,
		0xe585,	0xe58c,	0xe594,	0xe5a4,	0xe5ab,	0xe5b6,	0xe5ea,	0xe563,	0xe5c6,	0xe630,	0xe696,	0xe6ba,	0xe6ca,	0xe725,	0xe7aa,	0xe903,
		0xe7db,	0xe80d,	0xe987,	0xe9d1,	0xe87d,	0xe905,	0xe965,	0xe974,	0xe994,	0xe9a9,	0xe9bb,	0xec45,	0xeccc,	0xedc4,	0xecc7,	0xed01,
		0xed09,	0xed70,	0xed81,	0xed8f,	0xe0ad,	0xeee8,	0xeef8,	0xebdf,	0xebe2,	0xebe5,	0xebeb,	0xebee,	0xebf4,	0xebf7,	0xebfa,	0xebe8,
		0xf43c,	0xf4ef,	0xf523,	0xf561,	0xf535,	0xf57b,	0xf5d3,	0xf71a,	0xf73f,	0xf7e4,	0xf7e0,	0xf82f,	0xf88f,	0xf8af,	0xf8b5,	0xf920,
		0xf967,	0xf960,	0xf9c9,	0xfa14,	0xfa85,	0xfa9b,	0xfab1,	0xfac7,	0xfafa,	0xfb10,	0xfb26,	0xfbb6,	0xfbfe
	};
	const std::set<uint16_t> variable_locations = {
		0x0228, 0x0229, 0x022a, 0x022b, 0x022c, 0x022d, 0x0230
	};

	return score(disassembly, rom_functions, variable_locations);
}

int basic11_score(const Analyser::Static::MOS6502::Disassembly &disassembly) {
	const std::set<uint16_t> rom_functions = {
		0x0238,	0x023b,	0x023e,	0x0241,	0x0244,	0x0247,
		0xc3c6,	0xc3f4,	0xc444,	0xc47c,	0xc4a8,	0xc4d3,	0xc4e0,	0xc524,	0xc55f,	0xc592,	0xc5e8,	0xc5fa,	0xc692,	0xc6b3,	0xc6ee,	0xc70d,
		0xc748,	0xc7fd,	0xc809,	0xc816,	0xc82f,	0xc855,	0xc8c1,	0xc915,	0xc952,	0xc971,	0xc973,	0xc9a0,	0xc9bd,	0xc9c8,	0xc9e5,	0xca12,
		0xca3c,	0xca4e,	0xca51,	0xca70,	0xca99,	0xcac2,	0xcae2,	0xcb1c,	0xcbab,	0xcbf0,	0xcc59,	0xccb0,	0xccce,	0xcd16,	0xcd19,	0xcd46,
		0xcd55,	0xcd89,	0xce98,	0xcf03,	0xcf17,	0xcfac,	0xd000,	0xd03c,	0xd059,	0xd07c,	0xd113,	0xd17e,	0xd188,	0xd361,	0xd3eb,	0xd47e,
		0xd4a6,	0xd4ba,	0xd593,	0xd5a3,	0xd5b5,	0xd650,	0xd730,	0xd767,	0xd816,	0xd82a,	0xd856,	0xd861,	0xd8a6,	0xd8b5,	0xd8c5,	0xd922,
		0xd938,	0xd94f,	0xd958,	0xd967,	0xd983,	0xd993,	0xd9b5,	0xd9de,	0xda0c,	0xda3f,	0xda51,	0xdaa1,	0xdaab,	0xdada,	0xdaf6,	0xdb92,
		0xdbb9,	0xdcaf,	0xdd51,	0xdda7,	0xddc3,	0xddd4,	0xde77,	0xdef4,	0xdf0b,	0xdf0f,	0xdf13,	0xdf21,	0xdf49,	0xdf4c,	0xdf8c,	0xdfbd,
		0xdfe7,	0xe076,	0xe0c5,	0xe22e,	0xe27c,	0xe2aa,	0xe313,	0xe34f,	0xe38b,	0xe392,	0xe3db,	0xe407,	0xe43f,	0xe46f,	0xe4ac,	0xe4e0,
		0xe4f2,	0xe56c,	0xe57d,	0xe585,	0xe58c,	0xe594,	0xe5a4,	0xe5ab,	0xe5b6,	0xe5ea,	0xe5f5,	0xe607,	0xe65e,	0xe6c9,	0xe735,	0xe75a,
		0xe76a,	0xe7b2,	0xe85b,	0xe903,	0xe909,	0xe946,	0xe987,	0xe9d1,	0xeaf0,	0xeb78,	0xebce,	0xebe7,	0xec0c,	0xec21,	0xec33,	0xec45,
		0xeccc,	0xedc4,	0xede0,	0xee1a,	0xee22,	0xee8c,	0xee9d,	0xeeab,	0xeec9,	0xeee8,	0xeef8,	0xf0c8,	0xf0fd,	0xf110,	0xf11d,	0xf12d,
		0xf204,	0xf210,	0xf268,	0xf37f,	0xf495,	0xf4ef,	0xf523,	0xf561,	0xf590,	0xf5c1,	0xf602,	0xf71a,	0xf77c,	0xf7e4,	0xf816,	0xf865,
		0xf88f,	0xf8af,	0xf8b5,	0xf920,	0xf967,	0xf9aa,	0xf9c9,	0xfa14,	0xfa9f,	0xfab5,	0xfacb,	0xfae1,	0xfb14,	0xfb2a,	0xfb40,	0xfbd0,
		0xfc18
	};
	const std::set<uint16_t> variable_locations = {
		0x0244, 0x0245, 0x0246, 0x0247, 0x0248, 0x0249, 0x024a, 0x024b, 0x024c
	};

	return score(disassembly, rom_functions, variable_locations);
}

bool is_microdisc(Storage::Encodings::MFM::Parser &parser) {
	/*
		The Microdisc boot sector is sector 2 of track 0 and contains a 23-byte signature.
	*/
	Storage::Encodings::MFM::Sector *sector = parser.get_sector(0, 0, 2);
	if(!sector) return false;
	if(sector->samples.empty()) return false;

	const std::vector<uint8_t> &first_sample = sector->samples[0];
	if(first_sample.size() != 256) return false;

	const uint8_t signature[] = {
		0x00, 0x00, 0xFF, 0x00, 0xD0, 0x9F, 0xD0,
		0x9F, 0x02, 0xB9, 0x01, 0x00, 0xFF, 0x00,
		0x00, 0xB9, 0xE4, 0xB9, 0x00, 0x00, 0xE6,
		0x12, 0x00
	};

	return !std::memcmp(signature, first_sample.data(), sizeof(signature));
}

bool is_400_loader(Storage::Encodings::MFM::Parser &parser, uint16_t range_start, uint16_t range_end) {
	/*
		Both the Jasmin and BD-DOS boot sectors are sector 1 of track 0 and are loaded at $400;
		use disassembly to test for likely matches.
	*/

	Storage::Encodings::MFM::Sector *sector = parser.get_sector(0, 0, 1);
	if(!sector) return false;
	if(sector->samples.empty()) return false;

	// Take a copy of the first sampling, and keep only the final 256 bytes (assuming at least that many were found).
	std::vector<uint8_t> first_sample = sector->samples[0];
	if(first_sample.size() < 256) return false;
	if(first_sample.size() > 256) {
		first_sample.erase(first_sample.end() - 256, first_sample.end());
	}

	// Grab a disassembly.
	const auto disassembly =
		Analyser::Static::MOS6502::Disassemble(first_sample, Analyser::Static::Disassembler::OffsetMapper(0x400), {0x400});

	// Check for references to the Jasmin registers.
	int register_hits = 0;
	for(auto list : {disassembly.external_stores, disassembly.external_loads, disassembly.external_modifies}) {
		for(auto address : list) {
			register_hits += (address >= range_start && address <= range_end);
		}
	}

	// Arbitrary, sure, but as long as at least two accesses to the requested register range are found, accept this.
	return register_hits >= 2;
}

bool is_jasmin(Storage::Encodings::MFM::Parser &parser) {
	return is_400_loader(parser, 0x3f4, 0x3ff);
}

bool is_bd500(Storage::Encodings::MFM::Parser &parser) {
	return is_400_loader(parser, 0x310, 0x323);
}

}

Analyser::Static::TargetList Analyser::Static::Oric::GetTargets(const Media &media, const std::string &file_name, TargetPlatform::IntType potential_platforms) {
	auto target = std::make_unique<Target>();
	target->machine = Machine::Oric;
	target->confidence = 0.5;

	int basic10_votes = 0;
	int basic11_votes = 0;

	for(auto &tape : media.tapes) {
		std::vector<File> tape_files = GetFiles(tape);
		tape->reset();
		if(!tape_files.empty()) {
			for(const auto &file : tape_files) {
				if(file.data_type == File::MachineCode) {
					std::vector<uint16_t> entry_points = {file.starting_address};
					const Analyser::Static::MOS6502::Disassembly disassembly =
						Analyser::Static::MOS6502::Disassemble(file.data, Analyser::Static::Disassembler::OffsetMapper(file.starting_address), entry_points);

					if(basic10_score(disassembly) > basic11_score(disassembly)) ++basic10_votes; else ++basic11_votes;
				}
			}

			target->media.tapes.push_back(tape);
			target->loading_command = "CLOAD\"\"\n";
		}
	}

	if(!media.disks.empty()) {
		// 8-DOS is recognised by a dedicated Disk II analyser, so check only for Microdisc,
		// Jasmin and BD-DOS formats here.
		for(auto &disk: media.disks) {
			Storage::Encodings::MFM::Parser parser(true, disk);

			if(is_microdisc(parser)) {
				target->disk_interface = Target::DiskInterface::Microdisc;
				target->media.disks.push_back(disk);
			} else if(is_jasmin(parser)) {
				target->disk_interface = Target::DiskInterface::Jasmin;
				target->should_start_jasmin = true;
				target->media.disks.push_back(disk);
			} else if(is_bd500(parser)) {
				target->disk_interface = Target::DiskInterface::BD500;
				target->media.disks.push_back(disk);
			}
		}
	}
	else
		target->disk_interface = Target::DiskInterface::None;

	// TODO: really this should add two targets if not all votes agree
	if(basic11_votes >= basic10_votes || target->disk_interface == Target::DiskInterface::Microdisc)
		target->rom = Target::ROM::BASIC11;
	else
		target->rom = Target::ROM::BASIC10;

	TargetList targets;
	if(target->media.tapes.size() || target->media.disks.size() || target->media.cartridges.size())
		targets.push_back(std::move(target));
	return targets;
}
