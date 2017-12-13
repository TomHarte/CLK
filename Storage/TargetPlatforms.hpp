//
//  TargetPlatforms.h
//  Clock Signal
//
//  Created by Thomas Harte on 27/08/2017.
//  Copyright © 2017 Thomas Harte. All rights reserved.
//

#ifndef TargetPlatforms_hpp
#define TargetPlatforms_hpp

namespace TargetPlatform {

typedef int IntType;
enum Type: IntType {
	AmstradCPC		=	1 << 1,
	Atari2600		=	1 << 2,
	AcornAtom		=	1 << 3,
	AcornElectron	=	1 << 4,
	BBCMaster		=	1 << 5,
	BBCModelA		=	1 << 6,
	BBCModelB		=	1 << 7,
	Commodore		=	1 << 8,
	MSX				=	1 << 9,
	Oric			=	1 << 10,
	ZX80			=	1 << 11,
	ZX81			=	1 << 12,

	Acorn			=	AcornAtom | AcornElectron | BBCMaster | BBCModelA | BBCModelB,
	ZX8081			=	ZX80 | ZX81,
	AllTape			=	Acorn | AmstradCPC | Commodore | Oric | ZX80 | ZX81 | MSX,
	AllDisk			=	Acorn | AmstradCPC | Commodore | Oric | MSX,
};

class TypeDistinguisher {
	public:
		virtual Type target_platform_type() = 0;
};

}

#endif /* TargetPlatforms_h */
