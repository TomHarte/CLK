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
#include "Atari/StaticAnalyser.hpp"
#include "Commodore/StaticAnalyser.hpp"
#include "Oric/StaticAnalyser.hpp"
#include "ZX8081/StaticAnalyser.hpp"

// Cartridges
#include "../Storage/Cartridge/Formats/BinaryDump.hpp"
#include "../Storage/Cartridge/Formats/PRG.hpp"

// Disks
#include "../Storage/Disk/Formats/AcornADF.hpp"
#include "../Storage/Disk/Formats/D64.hpp"
#include "../Storage/Disk/Formats/G64.hpp"
#include "../Storage/Disk/Formats/OricMFMDSK.hpp"
#include "../Storage/Disk/Formats/SSD.hpp"

// Tapes
#include "../Storage/Tape/Formats/CommodoreTAP.hpp"
#include "../Storage/Tape/Formats/OricTAP.hpp"
#include "../Storage/Tape/Formats/TapePRG.hpp"
#include "../Storage/Tape/Formats/TapeUEF.hpp"
#include "../Storage/Tape/Formats/ZX80O.hpp"

typedef int TargetPlatformType;
enum class TargetPlatform: TargetPlatformType {
	Acorn		=	1 << 0,
	Atari2600	=	1 << 1,
	Commodore	=	1 << 2,
	Oric		=	1 << 3,
	ZX80		=	1 << 4,
};

using namespace StaticAnalyser;

std::list<Target> StaticAnalyser::GetTargets(const char *file_name)
{
	std::list<Target> targets;

	// Get the extension, if any; it will be assumed that extensions are reliable, so an extension is a broad-phase
	// test as to file format.
	const char *mixed_case_extension = strrchr(file_name, '.');
	char *lowercase_extension = nullptr;
	if(mixed_case_extension)
	{
		lowercase_extension = strdup(mixed_case_extension+1);
		char *parser = lowercase_extension;
		while(*parser)
		{
			*parser = (char)tolower(*parser);
			parser++;
		}
	}

	// Collect all disks, tapes and ROMs as can be extrapolated from this file, forming the
	// union of all platforms this file might be a target for.
	std::list<std::shared_ptr<Storage::Disk::Disk>> disks;
	std::list<std::shared_ptr<Storage::Tape::Tape>> tapes;
	std::list<std::shared_ptr<Storage::Cartridge::Cartridge>> cartridges;
	TargetPlatformType potential_platforms = 0;

#define Insert(list, class, platforms) \
	list.emplace_back(new Storage::class(file_name));\
	potential_platforms |= (TargetPlatformType)(platforms);\

#define TryInsert(list, class, platforms) \
	try {\
		Insert(list, class, platforms) \
	} catch(...) {}

#define Format(extension, list, class, platforms) \
	if(!strcmp(lowercase_extension, extension))	\
	{	\
		TryInsert(list, class, platforms)	\
	}

	if(lowercase_extension)
	{
		Format("80", tapes, Tape::ZX80O, TargetPlatform::ZX80)							// 80
		Format("a26", cartridges, Cartridge::BinaryDump, TargetPlatform::Atari2600)		// A26
		Format("adf", disks, Disk::AcornADF, TargetPlatform::Acorn)						// ADF
		Format("bin", cartridges, Cartridge::BinaryDump, TargetPlatform::Atari2600)		// BIN
		Format("d64", disks, Disk::D64, TargetPlatform::Commodore)						// D64
		Format("dsd", disks, Disk::SSD, TargetPlatform::Acorn)							// DSD
		Format("dsk", disks, Disk::OricMFMDSK, TargetPlatform::Oric)					// DSK
		Format("g64", disks, Disk::G64, TargetPlatform::Commodore)						// G64
		Format("o", tapes, Tape::ZX80O, TargetPlatform::ZX80)							// O

		// PRG
		if(!strcmp(lowercase_extension, "prg"))
		{
			// try instantiating as a ROM; failing that accept as a tape
			try {
				Insert(cartridges, Cartridge::PRG, TargetPlatform::Commodore)
			}
			catch(...)
			{
				try {
					Insert(tapes, Tape::PRG, TargetPlatform::Commodore)
				} catch(...) {}
			}
		}

		Format("rom", cartridges, Cartridge::BinaryDump, TargetPlatform::Acorn)		// ROM
		Format("ssd", disks, Disk::SSD, TargetPlatform::Acorn)						// SSD
		Format("tap", tapes, Tape::CommodoreTAP, TargetPlatform::Commodore)			// TAP (Commodore)
		Format("tap", tapes, Tape::OricTAP, TargetPlatform::Oric)					// TAP (Oric)
		Format("uef", tapes, Tape::UEF, TargetPlatform::Acorn)						// UEF (tape)

#undef Format
#undef Insert
#undef TryInsert

		// Hand off to platform-specific determination of whether these things are actually compatible and,
		// if so, how to load them. (TODO)
		if(potential_platforms & (TargetPlatformType)TargetPlatform::Acorn)		Acorn::AddTargets(disks, tapes, cartridges, targets);
		if(potential_platforms & (TargetPlatformType)TargetPlatform::Atari2600)	Atari::AddTargets(disks, tapes, cartridges, targets);
		if(potential_platforms & (TargetPlatformType)TargetPlatform::Commodore)	Commodore::AddTargets(disks, tapes, cartridges, targets);
		if(potential_platforms & (TargetPlatformType)TargetPlatform::Oric)		Oric::AddTargets(disks, tapes, cartridges, targets);
		if(potential_platforms & (TargetPlatformType)TargetPlatform::ZX80)		ZX8081::AddTargets(disks, tapes, cartridges, targets);

		free(lowercase_extension);
	}

	// Reset any tapes to their initial position
	for(auto target : targets) {
		for(auto tape : target.tapes) {
			tape->reset();
		}
	}

	return targets;
}
