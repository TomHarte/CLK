//
//  StaticAnalyser.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 23/08/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#include "StaticAnalyser.hpp"

#include <cstdlib>

#include "../Storage/Cartridge/Formats/PRG.hpp"

#include "../Storage/Disk/Formats/D64.hpp"
#include "../Storage/Disk/Formats/G64.hpp"

#include "../Storage/Tape/Formats/CommodoreTAP.hpp"
#include "../Storage/Tape/Formats/TapePRG.hpp"
#include "../Storage/Tape/Formats/TapeUEF.hpp"

typedef int TargetPlatformType;
enum class TargetPlatform: TargetPlatformType {
	Acorn		=	1 << 0,
	Atari2600	=	1 << 1,
	Commodore	=	1 << 2
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

#define Format(extension, list, class, platforms) \
	if(!strcmp(lowercase_extension, extension))	\
	{	\
		try {	\
			list.emplace_back(new Storage::class(file_name));\
			potential_platforms |= (TargetPlatformType)(platforms);\
		} catch(...) {}\
	}

	// A26
	if(!strcmp(lowercase_extension, "a26"))
	{
	}

	// BIN
	if(!strcmp(lowercase_extension, "bin"))
	{
	}

	Format("d64", disks, Disk::D64, TargetPlatform::Commodore)		// D64
	Format("g64", disks, Disk::G64, TargetPlatform::Commodore)		// G64

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

	// ROM
	if(!strcmp(lowercase_extension, "rom"))
	{
	}

	Format("tap", tapes, Tape::CommodoreTAP, TargetPlatform::Commodore)	// TAP
	Format("uef", tapes, Tape::UEF, TargetPlatform::Acorn)				// UEF (tape)

#undef Format
#undef Insert

	// Hand off to platform-specific determination of whether these things are actually compatible and,
	// if so, how to load them. (TODO)

	printf("Lowercase extension: %s", lowercase_extension);

	free(lowercase_extension);
	return targets;
}
