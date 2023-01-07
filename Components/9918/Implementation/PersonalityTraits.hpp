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

constexpr bool is_classic_vdp(Personality p) {
	return
		p == Personality::TMS9918A ||
		p == Personality::SMSVDP ||
		p == Personality::SMS2VDP ||
		p == Personality::GGVDP;
}

}
}


#endif /* PersonalityTraits_hpp */
