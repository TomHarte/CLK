//
//  Storage.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 10/07/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#ifndef Storage_hpp
#define Storage_hpp

#include "../NumberTheory/Factors.hpp"

namespace Storage {

struct Time {
	unsigned int length, clock_rate;

	inline void simplify()
	{
		unsigned int common_divisor = NumberTheory::greatest_common_divisor(length, clock_rate);
		length /= common_divisor;
		clock_rate /= common_divisor;
	}
};

}

#endif /* Storage_h */
