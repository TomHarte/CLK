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
	Acorn		=	1 << 0,
	AmstradCPC	=	1 << 1,
	Atari2600	=	1 << 2,
	Commodore	=	1 << 3,
	Oric		=	1 << 4,
	ZX80		=	1 << 5,
	ZX81		=	1 << 6,

	ZX8081		= ZX80 | ZX81,
	AllTape		= Acorn | AmstradCPC | Commodore | Oric | ZX80 | ZX81,
	AllDisk		= Acorn | AmstradCPC | Commodore | Oric,
};

class TypeDistinguisher {
	public:
		virtual Type target_platform_type() = 0;
};

}

#endif /* TargetPlatforms_h */
