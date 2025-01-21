//
//  TargetPlatforms.h
//  Clock Signal
//
//  Created by Thomas Harte on 27/08/2017.
//  Copyright 2017 Thomas Harte. All rights reserved.
//

#pragma once

namespace TargetPlatform {

using IntType = int;

constexpr IntType bit(int index) {
	return 1 << index;
}

// The below is somehwat overspecified because some of the file formats already supported by this
// emulator can self-specify platforms beyond those the emulator otherwise implements.
enum Type: IntType {
	AcornAtom		=	bit(0),
	AcornElectron	=	bit(1),
	Amiga			=	bit(2),
	AmstradCPC		=	bit(3),
	AppleII			=	bit(4),
	AppleIIgs		=	bit(5),
	Archimedes		=	bit(6),
	Atari2600		=	bit(7),
	AtariST			=	bit(8),
	BBCMaster		=	bit(9),
	BBCModelA		=	bit(10),
	BBCModelB		=	bit(11),
	C64				=	bit(12),
	Coleco			=	bit(13),
	DiskII			=	bit(14),
	Enterprise		=	bit(15),
	FAT12			=	bit(16),
	Macintosh		=	bit(17),
	MSX				=	bit(18),
	Oric			=	bit(19),
	PCCompatible	=	bit(20),
	Plus4			=	bit(21),
	Sega			=	bit(22),
	Vic20			=	bit(23),
	ZX80			=	bit(24),
	ZX81			=	bit(25),
	ZXSpectrum		=	bit(26),

	Acorn			=	AcornAtom | AcornElectron | BBCMaster | BBCModelA | BBCModelB | Archimedes,
	Commodore8bit	=	C64 | Plus4 | Vic20,
	Commodore		=	Amiga | Commodore8bit,
	ZX8081			=	ZX80 | ZX81,

	AllCartridge	=	Atari2600 | AcornElectron | Coleco | MSX,
	AllDisk			=	Acorn | Commodore | AmstradCPC | C64 | Oric | MSX | ZXSpectrum | Macintosh | AtariST | DiskII | PCCompatible | FAT12,
	AllTape			=	Acorn | AmstradCPC | Commodore8bit | Oric | ZX8081 | MSX | ZXSpectrum,

	All 			= ~0,
};

class Distinguisher {
public:
	virtual Type target_platforms() = 0;
};

class Recipient {
public:
	virtual void set_target_platforms(Type) = 0;
};

}
