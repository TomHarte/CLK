//
//  6821.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 19/03/2026.
//  Copyright © 2026 Thomas Harte. All rights reserved.
//

#ifndef _821_hpp
#define _821_hpp

#include "Numeric/SizedInt.hpp"

namespace Motorola::MC6821 {

template <typename BusHandlerT>
class MC6821 {
public:
	void write(const Numeric::SizedInt<2> address, const uint8_t value) {
		(void)address;
		(void)value;
	}

	uint8_t read(const Numeric::SizedInt<2> address) {
		(void)address;
		return 0;
	}
};

};

#endif /* _821_hpp */
