//
//  Factors.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 29/07/2016.
//  Copyright 2016 Thomas Harte. All rights reserved.
//

#ifndef Factors_hpp
#define Factors_hpp

#include <numeric>
#include <utility>

namespace Numeric {
	/*!
		@returns The greatest common divisor of @c a and @c b.
	*/
	template<class T> T greatest_common_divisor(T a, T b) {
		return std::gcd(a, b);
	}

	/*!
		@returns The least common multiple of @c a and @c b computed indirectly via the greatest
		common divisor.
	*/
	template<class T> T least_common_multiple(T a, T b) {
		return std::lcm(a, b);
	}
}

#endif /* Factors_hpp */
