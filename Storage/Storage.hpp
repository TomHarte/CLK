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
	Time() : length(0), clock_rate(1) {}

	/*!
		Reduces this @c Time to its simplest form — eliminates all common factors from @c length
		and @c clock_rate.
	*/
	inline Time &simplify()
	{
		unsigned int common_divisor = NumberTheory::greatest_common_divisor(length, clock_rate);
		length /= common_divisor;
		clock_rate /= common_divisor;
		return *this;
	}

	/*!
		@returns the floating point conversion of this @c Time. This will often be less precise.
	*/
	inline float get_float()
	{
		return (float)length / (float)clock_rate;
	}

	inline bool operator < (Time other)
	{
		return other.clock_rate * length < clock_rate * other.length;
	}

	inline Time operator + (Time other)
	{
		Time result;
		result.clock_rate = NumberTheory::least_common_multiple(clock_rate, other.clock_rate);
		result.length = length * (result.clock_rate / clock_rate) + other.length * (result.clock_rate / other.clock_rate);
		return result;
	}

	inline Time &operator += (Time other)
	{
		unsigned int combined_clock_rate = NumberTheory::least_common_multiple(clock_rate, other.clock_rate);
		length = length * (combined_clock_rate / clock_rate) + other.length * (combined_clock_rate / other.clock_rate);
		clock_rate = combined_clock_rate;
		return *this;
	}

	inline Time operator - (Time other)
	{
		Time result;
		result.clock_rate = NumberTheory::least_common_multiple(clock_rate, other.clock_rate);
		result.length = length * (result.clock_rate / clock_rate) - other.length * (result.clock_rate / other.clock_rate);
		return result;
	}

	inline Time operator -= (Time other)
	{
		unsigned int combined_clock_rate = NumberTheory::least_common_multiple(clock_rate, other.clock_rate);
		length = length * (combined_clock_rate / clock_rate) - other.length * (combined_clock_rate / other.clock_rate);
		clock_rate = combined_clock_rate;
		return *this;
	}

	inline Time operator * (Time other)
	{
		Time result;
		result.clock_rate = clock_rate * other.clock_rate;
		result.length = length * other.length;
		return result;
	}

	inline Time &operator *= (Time other)
	{
		length *= other.length;
		clock_rate *= other.clock_rate;
		return *this;
	}

	inline Time operator / (Time other)
	{
		Time result;
		result.clock_rate = clock_rate * other.length;
		result.length = length * other.clock_rate;
		return result;
	}

	inline Time &operator /= (Time other)
	{
		length *= other.clock_rate;
		clock_rate *= other.length;
		return *this;
	}

	inline void set_zero()
	{
		length = 0;
		clock_rate = 1;
	}

	inline void set_one()
	{
		length = 1;
		clock_rate = 1;
	}
};


}

#endif /* Storage_h */
