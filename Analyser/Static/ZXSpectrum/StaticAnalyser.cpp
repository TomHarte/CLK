//
//  StaticAnalyser.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 17/03/2021.
//  Copyright © 2021 Thomas Harte. All rights reserved.
//

#include "StaticAnalyser.hpp"

#include "Storage/Disk/Parsers/CPM.hpp"
#include "Storage/Disk/Encodings/MFM/Parser.hpp"
#include "Storage/Tape/Parsers/Spectrum.hpp"

#include "Target.hpp"

#include <array>
#include <algorithm>

namespace {

enum class KnownGame {
	Aquaplane,
	AticAtac,
	BCsQuestForTyres,		// The real game is spelt the American way, but I can't bring myself to type it.
	DarkStar,
	Deathchase,
	FantasyWorldDizzy,
	JetPac,
	KnightLore,
	MatchDayII,
	SabreWulf,
	Underwurlde,
};

namespace Signatures {

// Spritework.
constexpr std::array atic_atac{
	0x68, 0xe2, 0x68, 0xe2, 0x68, 0xe2, 0x68, 0xe2, 0x68, 0xe2, 0x68, 0xe2,
	0x68, 0xe2, 0x68, 0xe2, 0x68, 0xe2, 0x68, 0xe2, 0x68, 0xe2, 0x68, 0xe2,
	0x68, 0xe2, 0x68, 0xe2, 0x68, 0xe2, 0x68, 0xed, 0x28, 0x0d, 0x28, 0x0d,
	0x28, 0x0d, 0x28, 0x0d, 0x28, 0x0d, 0x28, 0x0d, 0x28, 0x0d, 0x28, 0x0d,
	0x28, 0x0d, 0x28, 0x0d, 0x28, 0x0d, 0x28, 0x0d, 0x28, 0x0d, 0x28, 0x0d,
	0x28, 0x0d, 0x28,
};

// Text used on Aquaplane's title screen.
constexpr std::string_view aquaplane = "A Q U A P L A N E";

// The status line.
constexpr std::string_view bcs_quest_for_tyres = "WHEELS SPEED  SCORE  HIGH SCOREP1  00P2  00GAMEOVER";

// Part of the instructions.
constexpr std::string_view dark_star =
	"If it moves shoot it\r\rIf it doesn't shoot it anyway\r\rIf it's square fly through it !";

// Round names.
constexpr std::string_view deathchase = "DAY   PATROLNIGHT PATROL";

// Dialogue revealing that Daisy has been eggnapped.
constexpr std::array fantasy_world_dizzy{
	0x44, 0x41, 0x49, 0x53, 0x59, 0x3b, 0x53, 0x3a, 0x42, 0x45, 0x45, 0x4e,
	0x3a, 0x45, 0x47, 0x47, 0x4e, 0x41, 0x50, 0x50, 0x45, 0x44, 0x88, 0x50,
	0x41, 0x4e, 0x44, 0x3a, 0x49, 0x53, 0x3a, 0x42, 0x45, 0x49, 0x4e, 0x47,
	0x3a, 0x48, 0x45, 0x4c, 0x44, 0x3a, 0x49, 0x4e, 0x3a, 0x54, 0x48, 0x45,
	0x8a, 0x58, 0x57, 0x49, 0x5a, 0x41, 0x52, 0x44, 0x3b, 0x53, 0x3a, 0x43,
	0x4c, 0x4f, 0x55, 0x44, 0x3a, 0x43, 0x41, 0x53, 0x54, 0x4c, 0x45, 0x3c,
};

// Game copyright message.
constexpr std::array jet_pac{
	0x43, 0x4f, 0x50, 0x59, 0x52, 0x49, 0x47, 0x48, 0x54, 0x20, 0x31, 0x39,
	0x38, 0x33, 0x20, 0x41, 0x2e, 0x43, 0x2e, 0x47, 0x2e, 0x20, 0x41, 0x4c,
	0x4c, 0x20, 0x52, 0x49, 0x47, 0x48, 0x54, 0x53, 0x20, 0x52, 0x45, 0x53,
	0x45, 0x52, 0x56, 0x45, 0x44,
};

// Graphical data, I think.
constexpr std::array knight_lore{
	0x44, 0x5c, 0x5c, 0x5c, 0x58, 0x58, 0x6b, 0x6b, 0x68, 0x68, 0x55, 0x55,
	0x5a, 0x5a, 0x58, 0x58, 0x58, 0x4b, 0x49, 0x4f, 0x56, 0x56, 0x56, 0x56,
	0x00, 0x00, 0x47, 0x47, 0x46, 0x46, 0x46, 0x46, 0x43, 0x43, 0x44, 0x5c,
	0x5c, 0x4b, 0x4b, 0x58, 0x6b, 0x4d, 0x4d, 0x55, 0x55, 0x55, 0x5a, 0x5a,
	0x5a, 0x58, 0x4b, 0x49, 0x56, 0x56, 0x56, 0x56, 0x46, 0x46, 0x47, 0x47,
	0x46, 0x46, 0x47, 0x43, 0x43, 0x43, 0x46, 0x44, 0x5c, 0x58, 0x4b, 0x4b,
	0x4b, 0x49, 0x4d, 0x4d, 0x4d, 0x4a, 0x4a, 0x4a, 0x4a, 0x5a, 0x58, 0x4b,
	0x56, 0x56, 0x56, 0x56, 0x46, 0x47, 0x47, 0x00, 0x00, 0x46, 0x43, 0x7b,
	0x7b, 0x47, 0x47, 0x44, 0x5c, 0x5c, 0x58, 0x4b, 0x4f, 0x4f, 0x4f, 0x4f,
	0x4f, 0x4f, 0x4f, 0x4f, 0x4f, 0x4a, 0x5a, 0x58, 0x56, 0x56, 0x56, 0x56,
	0x46, 0x47, 0x00, 0x00, 0x00, 0x46, 0x7b, 0x7b, 0x47, 0x47, 0x46, 0x00,
	0x44, 0x5c, 0x58, 0x4b, 0x4f, 0x4f, 0x4f, 0x4f, 0x4f, 0x4f, 0x4f, 0x4f,
	0x4f, 0x4a, 0x5a, 0x58, 0x56, 0x56, 0x56, 0x56, 0x47, 0x47, 0x47, 0x47,
	0x47, 0x43, 0x7b, 0x7b, 0x7b, 0x47, 0x00, 0x47, 0x44, 0x44, 0x5c, 0x4b,
	0x4f, 0x4f, 0x4f, 0x4f, 0x4f, 0x4f, 0x4f, 0x4f, 0x4f, 0x4a, 0x5a, 0x58,
};

// Text (with specialised control characters) including the author's names and some football-related words.
constexpr std::array match_day_ii{
	0x82, 0x86, 0x1a, 0x00, 0x42, 0x45, 0x52, 0x4e, 0x49, 0x45, 0x06, 0x18,
	0x02, 0x44, 0x52, 0x55, 0x4d, 0x4d, 0x4f, 0x4e, 0x44, 0x06, 0x00, 0x00,
	0x4a, 0x4f, 0x4e, 0x06, 0x00, 0x02, 0x52, 0x49, 0x54, 0x4d, 0x41, 0x4e,
	0xff, 0x20, 0x48, 0x41, 0x4c, 0x46, 0xff, 0x54, 0x49, 0x4d, 0x45, 0xff,
	0x20, 0x45, 0x58, 0x54, 0x52, 0x41, 0x20, 0xff, 0x46, 0x49, 0x52, 0x53,
	0x54, 0xa3, 0xff, 0x53, 0x45, 0x43, 0x4f, 0x4e, 0x44,
};

// Likely to be part of the graphics.
constexpr std::array sabre_wulf{
	0x5c, 0x3a, 0x5c, 0x3a, 0x5c, 0x3a, 0x5c, 0x3a, 0x5c, 0x1f, 0x5c, 0x42,
	0x52, 0xd5, 0x5b, 0xd5, 0x5b, 0xd5, 0x5b, 0xd5, 0x5b, 0xd5, 0x5b, 0xc4,
	0x5b, 0x42, 0x52, 0x42, 0x52, 0x30, 0x51, 0x30, 0x51, 0x30, 0x51, 0x30,
	0x51, 0x30, 0x51, 0x30, 0x51, 0x30, 0x51, 0x30, 0x51, 0x30, 0x51, 0x30,
	0x51, 0x30, 0x51, 0x30, 0x51, 0x30, 0x51, 0x30, 0x51, 0x30, 0x51, 0x30,
	0x51, 0x2f, 0x52, 0x2f, 0x52, 0x2f, 0x52, 0x2f, 0x52, 0x2f, 0x52, 0x2f,
	0x52, 0x2f, 0x52, 0x2f, 0x52, 0x2f, 0x52, 0x2f, 0x52, 0x2f, 0x52, 0x2f,
	0x52, 0x2f, 0x52, 0x2f, 0x52, 0x2f, 0x52, 0x2f,
};

// Congratulations text.
constexpr std::array underwurlde{
	0x59, 0x4f, 0x55, 0x20, 0x48, 0x41, 0x56, 0x45, 0x20, 0x45, 0x53, 0x43,
	0x41, 0x50, 0x45, 0x44, 0x20, 0x46, 0x52, 0x4f, 0x4d, 0xa0, 0x54, 0x48,
	0x45, 0x20, 0x55, 0x4e, 0x44, 0x45, 0x52, 0x57, 0x55, 0x52, 0x4c, 0x44,
	0x45,
};

}

std::optional<KnownGame> known_game(const std::vector<uint8_t> &code) {
	const auto contains = [&](auto &&signature) {
		return std::search(code.begin(), code.end(), signature.begin(), signature.end()) != code.end();
	};

	if(contains(Signatures::atic_atac))				return KnownGame::AticAtac;
	if(contains(Signatures::aquaplane))				return KnownGame::Aquaplane;
	if(contains(Signatures::bcs_quest_for_tyres))	return KnownGame::BCsQuestForTyres;
	if(contains(Signatures::dark_star))				return KnownGame::DarkStar;
	if(contains(Signatures::deathchase))			return KnownGame::Deathchase;
	if(contains(Signatures::fantasy_world_dizzy))	return KnownGame::FantasyWorldDizzy;
	if(contains(Signatures::jet_pac))				return KnownGame::JetPac;
	if(contains(Signatures::knight_lore))			return KnownGame::KnightLore;
	if(contains(Signatures::match_day_ii))			return KnownGame::MatchDayII;
	if(contains(Signatures::sabre_wulf))			return KnownGame::SabreWulf;
	if(contains(Signatures::underwurlde))			return KnownGame::Underwurlde;

	return std::nullopt;
}

std::optional<KnownGame> known_game(Storage::Tape::TapeSerialiser &tape) {
	using Parser = Storage::Tape::ZXSpectrum::Parser;
	using BlockType = Storage::Tape::ZXSpectrum::BlockType;

	Parser parser(Parser::MachineType::ZXSpectrum);

	while(true) {
		const auto block = parser.find_block(tape);
		if(!block) break;

		// Check for a Spectrum contents block.
		if(block->type == BlockType::Body) {
			const auto game = known_game(parser.get_block_body(tape));
			if(game.has_value()) return game;
		}
	}

	return std::nullopt;
}

bool is_tape(Storage::Tape::TapeSerialiser &tape) {
	using Parser = Storage::Tape::ZXSpectrum::Parser;
	using BlockType = Storage::Tape::ZXSpectrum::BlockType;

	Parser parser(Parser::MachineType::ZXSpectrum);

	while(true) {
		const auto block = parser.find_block(tape);
		if(!block) break;
		if(block->type == BlockType::Program) {
			return true;
		}
	}

	return false;
}

bool is_disk(const Storage::Disk::Disk &disk) {
	Storage::Encodings::MFM::Parser parser(Storage::Encodings::MFM::Density::Double, disk);

	// Grab absolutely any sector from the first track to determine general encoding.
	const Storage::Encodings::MFM::Sector *any_sector = parser.any_sector(0, 0);
	if(!any_sector) return false;

	// Determine the sector base and get logical sector 1.
	const uint8_t sector_base = any_sector->address.sector & 0xc0;
	const Storage::Encodings::MFM::Sector *boot_sector = parser.sector(0, 0, sector_base + 1);
	if(!boot_sector) return false;

	Storage::Disk::CPM::ParameterBlock cpm_format{};
	switch(sector_base) {
		case 0x40:	cpm_format = Storage::Disk::CPM::ParameterBlock::cpc_system_format();	break;
		case 0xc0:	cpm_format = Storage::Disk::CPM::ParameterBlock::cpc_data_format();		break;

		default: {
			// Check the first ten bytes of the first sector for the disk format; if these are all
			// the same value then instead substitute a default format.
			std::array<uint8_t, 10> format;
			std::copy_n(boot_sector->samples[0].begin(), 10, format.begin());
			if(std::all_of(format.begin(), format.end(), [&](const uint8_t v) { return v == format[0]; })) {
				format = {0x00, 0x00, 0x28, 0x09, 0x02, 0x01, 0x03, 0x02, 0x2a, 0x52};
			}

			// Parse those ten bytes as:
			//
			// Byte 0: disc type
			// Byte 1: sidedness
			//		bits 0-6: arrangement
			//			0 => single sided
			//			1 => double sided, flip sides
			//			2 => double sided, up and over
			//		bit 7: double-track
			// Byte 2: number of tracks per side
			// Byte 3: number of sectors per track
			// Byte 4: Log2(sector size) - 7
			// Byte 5: number of reserved tracks
			// Byte 6: Log2(block size) - 7
			// Byte 7: number of directory blocks
			// Byte 8: gap length (read/write)
			// Byte 9: gap length(format)
			cpm_format.sectors_per_track = format[3];
			cpm_format.tracks = format[2];
			cpm_format.block_size = 128 << format[6];
			cpm_format.first_sector = sector_base + 1;
			cpm_format.reserved_tracks = format[5];

			// i.e. bits set downward from 0x4000 for as many blocks as form the catalogue.
			cpm_format.catalogue_allocation_bitmap = 0x8000 - (0x8000 >> format[7]);
		} break;
	}

	// If the boot sector sums to 3 modulo 256 then this is a Spectrum disk.
	const auto byte_sum = static_cast<uint8_t>(
		std::accumulate(boot_sector->samples[0].begin(), boot_sector->samples[0].end(), 0));
	if(byte_sum == 3) {
		return true;
	}

	// ... otherwise read a CPM directory and look for a BASIC program called "DISK".
	const auto catalogue = Storage::Disk::CPM::GetCatalogue(disk, cpm_format, false);
	return catalogue && catalogue->is_zx_spectrum_booter();
}

}

Analyser::Static::TargetList Analyser::Static::ZXSpectrum::GetTargets(
	const Media &media,
	const std::filesystem::path &,
	TargetPlatform::IntType,
	bool
) {
	TargetList destination;
	auto target = std::make_unique<Target>();
	target->confidence = 0.5;

	std::optional<KnownGame> game;

	if(!media.tapes.empty()) {
		bool has_spectrum_tape = false;
		for(auto &tape: media.tapes) {
			auto serialiser = tape->serialiser();

			const bool is_spectrum = is_tape(*serialiser);
			has_spectrum_tape |= is_spectrum;
			if(is_spectrum) {
				serialiser->reset();
				const auto tape_game = known_game(*serialiser);
				if(tape_game.has_value()) {
					game = tape_game;
				}
			}
		}

		if(has_spectrum_tape) {
			target->media.tapes = media.tapes;

			if(game) {
				switch(*game) {
					using enum KnownGame;
					case Aquaplane:
					case AticAtac:
					case BCsQuestForTyres:
					case DarkStar:
					case Deathchase:
					case JetPac:
					case KnightLore:
					case MatchDayII:
					case SabreWulf:
					case Underwurlde:
						target->model = Target::Model::FortyEightK;
					break;

					case FantasyWorldDizzy:
						target->model = Target::Model::Plus2a;
					break;
				}
			} else {
				target->model = Target::Model::OneTwoEightK;
			}
		}
	}

	if(!media.disks.empty()) {
		bool has_spectrum_disk = false;

		for(auto &disk: media.disks) {
			has_spectrum_disk |= is_disk(*disk);
		}

		if(has_spectrum_disk) {
			target->media.disks = media.disks;
			target->model = Target::Model::Plus3;
		}
	}

	// If any media survived, add the target.
	if(!target->media.empty()) {
		target->should_hold_enter = true;		// To force entry into the 'loader' and thereby load the media.
		target->loading_command = L"J\"\"\n";;	// On a 16kb or 48kb, to load the [implicitly a] tape.
		destination.push_back(std::move(target));
	}

	return destination;
}
