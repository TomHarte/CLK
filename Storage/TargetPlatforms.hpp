//
//  TargetPlatforms.h
//  Clock Signal
//
//  Created by Thomas Harte on 27/08/2017.
//  Copyright 2017 Thomas Harte. All rights reserved.
//

#pragma once

namespace TargetPlatform {

typedef int IntType;

// The below is somehwat overspecified because some of the file formats already supported by this
// emulator can self-specify platforms beyond those the emulator otherwise implements.
enum Type: IntType {
	AmstradCPC		=	1 << 0,
	AppleII			=	1 << 1,
	AppleIIgs		=	1 << 2,
	Atari2600		=	1 << 3,
	AtariST			=	1 << 4,
	AcornAtom		=	1 << 5,
	AcornElectron	=	1 << 6,
	Amiga			=	1 << 7,
	Archimedes		=	1 << 8,
	BBCMaster		=	1 << 9,
	BBCModelA		=	1 << 10,
	BBCModelB		=	1 << 11,
	Coleco			=	1 << 12,
	Commodore		=	1 << 13,
	DiskII			=	1 << 14,
	Enterprise		=	1 << 15,
	Sega			=	1 << 16,
	Macintosh		=	1 << 17,
	MSX				=	1 << 18,
	Oric			=	1 << 19,
	ZX80			=	1 << 20,
	ZX81			=	1 << 21,
	ZXSpectrum		=	1 << 22,
	PCCompatible	=	1 << 23,
	FAT12			=	1 << 24,

	Acorn			=	AcornAtom | AcornElectron | BBCMaster | BBCModelA | BBCModelB | Archimedes,
	ZX8081			=	ZX80 | ZX81,
	AllCartridge	=	Atari2600 | AcornElectron | Coleco | MSX,
	AllDisk			=	Acorn | AmstradCPC | Commodore | Oric | MSX | ZXSpectrum | Macintosh | AtariST | DiskII | Amiga | PCCompatible | FAT12,
	AllTape			=	Acorn | AmstradCPC | Commodore | Oric | ZX8081 | MSX | ZXSpectrum,
};

class TypeDistinguisher {
	public:
		virtual Type target_platform_type() = 0;
};

}
