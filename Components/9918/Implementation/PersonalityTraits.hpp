//
//  PersonalityTraits.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 06/01/2023.
//  Copyright © 2023 Thomas Harte. All rights reserved.
//

#ifndef PersonalityTraits_hpp
#define PersonalityTraits_hpp

namespace TI {
namespace TMS {

// Genus determinants for the various personalityes.
constexpr bool is_sega_vdp(Personality p) {
	return p >= Personality::SMSVDP;
}

constexpr bool is_yamaha_vdp(Personality p) {
	return p == Personality::V9938 || p == Personality::V9958;
}

// i.e. one with the original internal timings.
constexpr bool is_classic_vdp(Personality p) {
	return
		p == Personality::TMS9918A ||
		p == Personality::SMSVDP ||
		p == Personality::SMS2VDP ||
		p == Personality::GGVDP;
}

constexpr size_t memory_size(Personality p) {
	switch(p) {
		case TI::TMS::TMS9918A:
		case TI::TMS::SMSVDP:
		case TI::TMS::SMS2VDP:
		case TI::TMS::GGVDP:	return 16 * 1024;
		case TI::TMS::MDVDP:	return 64 * 1024;
		case TI::TMS::V9938:	return 128 * 1024;
		case TI::TMS::V9958:	return 192 * 1024;
	}
}

}
}


#endif /* PersonalityTraits_hpp */
