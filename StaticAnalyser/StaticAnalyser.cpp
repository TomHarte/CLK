//
//  StaticAnalyser.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 23/08/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#include "StaticAnalyser.hpp"

#include <cstdlib>

// Analysers
#include "Acorn/StaticAnalyser.hpp"
#include "AmstradCPC/StaticAnalyser.hpp"
#include "Atari/StaticAnalyser.hpp"
#include "Commodore/StaticAnalyser.hpp"
#include "Oric/StaticAnalyser.hpp"
#include "ZX8081/StaticAnalyser.hpp"

// Cartridges
#include "../Storage/Cartridge/Formats/BinaryDump.hpp"
#include "../Storage/Cartridge/Formats/PRG.hpp"

// Disks
#include "../Storage/Disk/Formats/AcornADF.hpp"
#include "../Storage/Disk/Formats/CPCDSK.hpp"
#include "../Storage/Disk/Formats/D64.hpp"
#include "../Storage/Disk/Formats/G64.hpp"
#include "../Storage/Disk/Formats/HFE.hpp"
#include "../Storage/Disk/Formats/OricMFMDSK.hpp"
#include "../Storage/Disk/Formats/SSD.hpp"

// Tapes
#include "../Storage/Tape/Formats/CommodoreTAP.hpp"
#include "../Storage/Tape/Formats/CSW.hpp"
#include "../Storage/Tape/Formats/OricTAP.hpp"
#include "../Storage/Tape/Formats/TapePRG.hpp"
#include "../Storage/Tape/Formats/TapeUEF.hpp"
#include "../Storage/Tape/Formats/TZX.hpp"
#include "../Storage/Tape/Formats/ZX80O81P.hpp"

// Target Platform Types
#include "../Storage/TargetPlatforms.hpp"

using namespace StaticAnalyser;

static Media GetMediaAndPlatforms(const char *file_name, TargetPlatform::IntType &potential_platforms) {
	// Get the extension, if any; it will be assumed that extensions are reliable, so an extension is a broad-phase
	// test as to file format.
	const char *mixed_case_extension = strrchr(file_name, '.');
	char *lowercase_extension = nullptr;
	if(mixed_case_extension) {
		lowercase_extension = strdup(mixed_case_extension+1);
		char *parser = lowercase_extension;
		while(*parser) {
			*parser = (char)tolower(*parser);
			parser++;
		}
	}

	Media result;
#define Insert(list, class, platforms) \
	list.emplace_back(new Storage::class(file_name));\
	potential_platforms |= platforms;\

#define TryInsert(list, class, platforms) \
	try {\
		Insert(list, class, platforms) \
	} catch(...) {}

#define Format(extension, list, class, platforms) \
	if(!strcmp(lowercase_extension, extension))	{	\
		TryInsert(list, class, platforms)	\
	}

	if(lowercase_extension) {
		Format("80", result.tapes, Tape::ZX80O81P, TargetPlatform::ZX8081)						// 80
		Format("81", result.tapes, Tape::ZX80O81P, TargetPlatform::ZX8081)						// 81
		Format("a26", result.cartridges, Cartridge::BinaryDump, TargetPlatform::Atari2600)		// A26
		Format("adf", result.disks, Disk::AcornADF, TargetPlatform::Acorn)						// ADF
		Format("bin", result.cartridges, Cartridge::BinaryDump, TargetPlatform::Atari2600)		// BIN
		Format("cdt", result.tapes, Tape::TZX,	TargetPlatform::AmstradCPC)						// CDT
		Format("csw", result.tapes, Tape::CSW,	TargetPlatform::AllTape)						// CSW
		Format("d64", result.disks, Disk::D64, TargetPlatform::Commodore)						// D64
		Format("dsd", result.disks, Disk::SSD, TargetPlatform::Acorn)							// DSD
		Format("dsk", result.disks, Disk::CPCDSK, TargetPlatform::AmstradCPC)					// DSK (Amstrad CPC)
		Format("dsk", result.disks, Disk::OricMFMDSK, TargetPlatform::Oric)						// DSK (Oric)
		Format("g64", result.disks, Disk::G64, TargetPlatform::Commodore)						// G64
		Format("hfe", result.disks, Disk::HFE, TargetPlatform::AmstradCPC)						// HFE (TODO: plus other target platforms)
		Format("o", result.tapes, Tape::ZX80O81P, TargetPlatform::ZX8081)						// O
		Format("p", result.tapes, Tape::ZX80O81P, TargetPlatform::ZX8081)						// P
		Format("p81", result.tapes, Tape::ZX80O81P, TargetPlatform::ZX8081)						// P81

		// PRG
		if(!strcmp(lowercase_extension, "prg")) {
			// try instantiating as a ROM; failing that accept as a tape
			try {
				Insert(result.cartridges, Cartridge::PRG, TargetPlatform::Commodore)
			} catch(...) {
				try {
					Insert(result.tapes, Tape::PRG, TargetPlatform::Commodore)
				} catch(...) {}
			}
		}

		Format("rom", result.cartridges, Cartridge::BinaryDump, TargetPlatform::Acorn)		// ROM
		Format("ssd", result.disks, Disk::SSD, TargetPlatform::Acorn)						// SSD
		Format("tap", result.tapes, Tape::CommodoreTAP, TargetPlatform::Commodore)			// TAP (Commodore)
		Format("tap", result.tapes, Tape::OricTAP, TargetPlatform::Oric)					// TAP (Oric)
		Format("tzx", result.tapes, Tape::TZX, TargetPlatform::ZX8081)						// TZX
		Format("uef", result.tapes, Tape::UEF, TargetPlatform::Acorn)						// UEF (tape)

#undef Format
#undef Insert
#undef TryInsert

		free(lowercase_extension);
	}

	return result;
}

Media StaticAnalyser::GetMedia(const char *file_name) {
	TargetPlatform::IntType throwaway;
	return GetMediaAndPlatforms(file_name, throwaway);
}

std::list<Target> StaticAnalyser::GetTargets(const char *file_name) {
	std::list<Target> targets;

	// Collect all disks, tapes and ROMs as can be extrapolated from this file, forming the
	// union of all platforms this file might be a target for.
	TargetPlatform::IntType potential_platforms = 0;
	Media media = GetMediaAndPlatforms(file_name, potential_platforms);

	// Hand off to platform-specific determination of whether these things are actually compatible and,
	// if so, how to load them.
	if(potential_platforms & TargetPlatform::Acorn)			Acorn::AddTargets(media, targets);
	if(potential_platforms & TargetPlatform::AmstradCPC)	AmstradCPC::AddTargets(media, targets);
	if(potential_platforms & TargetPlatform::Atari2600)		Atari::AddTargets(media, targets);
	if(potential_platforms & TargetPlatform::Commodore)		Commodore::AddTargets(media, targets);
	if(potential_platforms & TargetPlatform::Oric)			Oric::AddTargets(media, targets);
	if(potential_platforms & TargetPlatform::ZX8081)		ZX8081::AddTargets(media, targets);

	// Reset any tapes to their initial position
	for(auto target : targets) {
		for(auto tape : media.tapes) {
			tape->reset();
		}
	}

	return targets;
}
