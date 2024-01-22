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
	BBCMaster		=	1 << 8,
	BBCModelA		=	1 << 9,
	BBCModelB		=	1 << 10,
	Coleco			=	1 << 11,
	Commodore		=	1 << 12,
	DiskII			=	1 << 13,
	Enterprise		=	1 << 14,
	Sega			=	1 << 15,
	Macintosh		=	1 << 16,
	MSX				=	1 << 17,
	Oric			=	1 << 18,
	ZX80			=	1 << 19,
	ZX81			=	1 << 20,
	ZXSpectrum		=	1 << 21,
	PCCompatible	=	1 << 22,
	FAT12			=	1 << 23,

	Acorn			=	AcornAtom | AcornElectron | BBCMaster | BBCModelA | BBCModelB,
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
