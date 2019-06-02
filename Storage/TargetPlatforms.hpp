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
enum Type: IntType {
	AmstradCPC		=	1 << 1,
	AppleII			=	1 << 2,
	Atari2600		=	1 << 3,
	AcornAtom		=	1 << 4,
	AcornElectron	=	1 << 5,
	BBCMaster		=	1 << 6,
	BBCModelA		=	1 << 7,
	BBCModelB		=	1 << 8,
	ColecoVision	=	1 << 9,
	Commodore		=	1 << 10,
	DiskII			=	1 << 11,
	Sega			=	1 << 12,
	Macintosh		=	1 << 13,
	MSX				=	1 << 14,
	Oric			=	1 << 15,
	ZX80			=	1 << 16,
	ZX81			=	1 << 17,

	Acorn			=	AcornAtom | AcornElectron | BBCMaster | BBCModelA | BBCModelB,
	ZX8081			=	ZX80 | ZX81,
	AllCartridge	=	Atari2600 | AcornElectron | ColecoVision | MSX,
	AllDisk			=	Acorn | AmstradCPC | Commodore | Oric | MSX,
	AllTape			=	Acorn | AmstradCPC | Commodore | Oric | ZX80 | ZX81 | MSX,
};

class TypeDistinguisher {
	public:
		virtual Type target_platform_type() = 0;
};

}

#endif /* TargetPlatforms_h */
