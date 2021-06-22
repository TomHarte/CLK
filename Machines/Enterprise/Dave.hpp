//
//  Dave.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 22/06/2021.
//  Copyright © 2021 Thomas Harte. All rights reserved.
//

#ifndef Dave_hpp
#define Dave_hpp

#include <cstdint>

#include "../../Numeric/LFSR.hpp"

namespace Enterprise {

/*!
	Models a subset of Dave's behaviour; memory mapping and interrupt status
	is integrated into the main Enterprise machine.
*/
class Dave {
	public:
		void write(uint16_t address, uint8_t value);

	private:

		// Various polynomials that contribute to audio generation.
		Numeric::LFSRv<0xc> poly4_;
		Numeric::LFSRv<0x14> poly5_;
		Numeric::LFSRv<0x60> poly7_;
		Numeric::LFSRv<0x110> poly9_;
		Numeric::LFSRv<0x500> poly11_;
		Numeric::LFSRv<0x6000> poly15_;
		Numeric::LFSRv<0x12000> poly17_;
};

}

#endif /* Dave_hpp */
