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
#include <cmath>
#include <cstdint>
#include <limits>

namespace Storage {

/*!
	Contains either an absolute time or a time interval, described as a quotient, in terms of a
	clock rate to which the time is relative and its length in cycles based on that clock rate.
*/
struct Time {
	unsigned int length, clock_rate;
	Time() : length(0), clock_rate(1) {}
	Time(unsigned int unsigned_int_value) : length(unsigned_int_value), clock_rate(1) {}
	Time(int int_value) : Time((unsigned int)int_value) {}
	Time(unsigned int length, unsigned int clock_rate) : length(length), clock_rate(clock_rate) { simplify(); }
	Time(int length, int clock_rate) : Time((unsigned int)length, (unsigned int)clock_rate) {}
	Time(uint64_t length, uint64_t clock_rate)
	{
		install_result(length, clock_rate);
		simplify();
	}
	Time(float value)
	{
		install_float(value);
		simplify();
	}

	/*!
		Reduces this @c Time to its simplest form — eliminates all common factors from @c length
		and @c clock_rate.
	*/
	void simplify()
	{
		unsigned int common_divisor = NumberTheory::greatest_common_divisor(length, clock_rate);
		length /= common_divisor;
		clock_rate /= common_divisor;
	}

	/*!
		@returns the floating point conversion of this @c Time. This will often be less precise.
	*/
	inline float get_float() const
	{
		return (float)length / (float)clock_rate;
	}

	inline unsigned int get_unsigned_int() const
	{
		return length / clock_rate;
	}

	inline bool operator < (const Time &other) const
	{
		return (uint64_t)other.clock_rate * (uint64_t)length < (uint64_t)clock_rate * (uint64_t)other.length;
	}

	inline bool operator <= (const Time &other) const
	{
		return (uint64_t)other.clock_rate * (uint64_t)length <= (uint64_t)clock_rate * (uint64_t)other.length;
	}

	inline bool operator > (const Time &other) const
	{
		return (uint64_t)other.clock_rate * (uint64_t)length > (uint64_t)clock_rate * (uint64_t)other.length;
	}

	inline bool operator >= (const Time &other) const
	{
		return (uint64_t)other.clock_rate * (uint64_t)length >= (uint64_t)clock_rate * (uint64_t)other.length;
	}

	inline bool operator == (const Time &other) const
	{
		return (uint64_t)other.clock_rate * (uint64_t)length == (uint64_t)clock_rate * (uint64_t)other.length;
	}

	inline Time operator + (const Time &other) const
	{
		uint64_t result_length = (uint64_t)length * (uint64_t)other.clock_rate + (uint64_t)other.length * (uint64_t)clock_rate;
		uint64_t result_clock_rate = (uint64_t)clock_rate * (uint64_t)other.clock_rate;
		return Time(result_length, result_clock_rate);
	}

	inline Time &operator += (const Time &other)
	{
		uint64_t result_length = (uint64_t)length * (uint64_t)other.clock_rate + (uint64_t)other.length * (uint64_t)clock_rate;
		uint64_t result_clock_rate = (uint64_t)clock_rate * (uint64_t)other.clock_rate;
		install_result(result_length, result_clock_rate);
		return *this;
	}

	inline Time operator - (const Time &other) const
	{
		uint64_t result_length = (uint64_t)length * (uint64_t)other.clock_rate - (uint64_t)other.length * (uint64_t)clock_rate;
		uint64_t result_clock_rate = (uint64_t)clock_rate * (uint64_t)other.clock_rate;
		return Time(result_length, result_clock_rate);
	}

	inline Time operator -= (const Time &other)
	{
		uint64_t result_length = (uint64_t)length * (uint64_t)other.clock_rate - (uint64_t)other.length * (uint64_t)clock_rate;
		uint64_t result_clock_rate = (uint64_t)clock_rate * (uint64_t)other.clock_rate;
		install_result(result_length, result_clock_rate);
		return *this;
	}

	inline Time operator * (const Time &other) const
	{
		uint64_t result_length = (uint64_t)length * (uint64_t)other.length;
		uint64_t result_clock_rate = (uint64_t)clock_rate * (uint64_t)other.clock_rate;
		return Time(result_length, result_clock_rate);
	}

	inline Time &operator *= (const Time &other)
	{
		uint64_t result_length = (uint64_t)length * (uint64_t)other.length;
		uint64_t result_clock_rate = (uint64_t)clock_rate * (uint64_t)other.clock_rate;
		install_result(result_length, result_clock_rate);
		return *this;
	}

	inline Time operator * (unsigned int multiplier) const
	{
		uint64_t result_length = (uint64_t)length * (uint64_t)multiplier;
		uint64_t result_clock_rate = (uint64_t)clock_rate;
		return Time(result_length, result_clock_rate);
	}

	inline Time &operator *= (unsigned int multiplier)
	{
		uint64_t result_length = (uint64_t)length * (uint64_t)multiplier;
		uint64_t result_clock_rate = (uint64_t)clock_rate;
		install_result(result_length, result_clock_rate);
		return *this;
	}

	inline Time operator / (const Time &other) const
	{
		uint64_t result_length = (uint64_t)length * (uint64_t)other.clock_rate;
		uint64_t result_clock_rate = (uint64_t)clock_rate * (uint64_t)other.length;
		return Time(result_length, result_clock_rate);
	}

	inline Time &operator /= (const Time &other)
	{
		uint64_t result_length = (uint64_t)length * (uint64_t)other.clock_rate;
		uint64_t result_clock_rate = (uint64_t)clock_rate * (uint64_t)other.length;
		install_result(result_length, result_clock_rate);
		return *this;
	}

	inline Time operator / (unsigned int divisor) const
	{
		uint64_t result_length = (uint64_t)length;
		uint64_t result_clock_rate = (uint64_t)clock_rate * (uint64_t)divisor;
		return Time(result_length, result_clock_rate);
	}

	inline Time &operator /= (unsigned int divisor)
	{
		uint64_t result_length = (uint64_t)length;
		uint64_t result_clock_rate = (uint64_t)clock_rate * (uint64_t)divisor;
		install_result(result_length, result_clock_rate);
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

	private:
		inline void install_result(uint64_t long_length, uint64_t long_clock_rate)
		{
			// TODO: switch to appropriate values if the result is too large or small to fit, even with trimmed accuracy.

			while(!(long_length&1) && !(long_clock_rate&1))
			{
				long_length >>= 1;
				long_clock_rate >>= 1;
			}

			if(long_length > std::numeric_limits<unsigned int>::max() || long_clock_rate > std::numeric_limits<unsigned int>::max())
			{
				uint64_t common_divisor = NumberTheory::greatest_common_divisor(long_length, long_clock_rate);
				long_length /= common_divisor;
				long_clock_rate /= common_divisor;

				// Okay, in desperation accept a loss of accuracy.
				while(long_length > std::numeric_limits<unsigned int>::max() || long_clock_rate > std::numeric_limits<unsigned int>::max())
				{
					long_length >>= 1;
					long_clock_rate >>= 1;
				}
			}
			length = (unsigned int)long_length;
			clock_rate = (unsigned int)long_clock_rate;
		}

		inline void install_float(float value)
		{
			int exponent;
			float mantissa = frexpf(value, &exponent);
			float loaded_mantissa = ldexpf(mantissa, 24);

			uint64_t result_length = (uint64_t)loaded_mantissa;
			uint64_t result_clock_rate = 1 << (exponent - 24);
			install_result(result_length, result_clock_rate);
		}
};


}

#endif /* Storage_h */
