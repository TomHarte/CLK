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

/*!
	Contains either an absolute time or a time interval, described as a quotient, in terms of a
	clock rate to which the time is relative and its length in cycles based on that clock rate.
*/
struct Time {
	unsigned int length, clock_rate;

	/*!
		Reduces this @c Time to its simplest form — eliminates all common factors from @c length
		and @c clock_rate.
	*/
	inline void simplify()
	{
		unsigned int common_divisor = NumberTheory::greatest_common_divisor(length, clock_rate);
		length /= common_divisor;
		clock_rate /= common_divisor;
	}

	/*!
		Returns the floating point conversion of this @c Time. This will often be less precise.
	*/
	inline float get_float()
	{
		return (float)length / (float)clock_rate;
	}
};

}

#endif /* Storage_h */
