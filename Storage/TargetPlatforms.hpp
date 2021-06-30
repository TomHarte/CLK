//
//  TargetPlatforms.h
//  Clock Signal
//
//  Created by Thomas Harte on 27/08/2017.
//  Copyright 2017 Thomas Harte. All rights reserved.
//

#ifndef TargetPlatforms_hpp
#define TargetPlatforms_hpp

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
	BBCMaster		=	1 << 7,
	BBCModelA		=	1 << 8,
	BBCModelB		=	1 << 9,
	Coleco			=	1 << 10,
	Commodore		=	1 << 11,
	DiskII			=	1 << 12,
	Enterprise		=	1 << 13,
	Sega			=	1 << 14,
	Macintosh		=	1 << 15,
	MSX				=	1 << 16,
	Oric			=	1 << 17,
	ZX80			=	1 << 18,
	ZX81			=	1 << 19,
	ZXSpectrum		=	1 << 20,

	Acorn			=	AcornAtom | AcornElectron | BBCMaster | BBCModelA | BBCModelB,
	ZX8081			=	ZX80 | ZX81,
	AllCartridge	=	Atari2600 | AcornElectron | Coleco | MSX,
	AllDisk			=	Acorn | AmstradCPC | Commodore | Oric | MSX | ZXSpectrum | Macintosh | AtariST | DiskII,
	AllTape			=	Acorn | AmstradCPC | Commodore | Oric | ZX8081 | MSX | ZXSpectrum,
};

class TypeDistinguisher {
	public:
		virtual Type target_platform_type() = 0;
};

}

#endif /* TargetPlatforms_h */
