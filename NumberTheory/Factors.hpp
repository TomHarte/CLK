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

namespace NumberTheory {
	/*!
		@returns The greatest common divisor of @c a and @c b.
	*/
	template<class T> T greatest_common_divisor(T a, T b) {
#if __cplusplus > 201402L
		return std::gcd(a, b);
#else
		if(a < b) {
			std::swap(a, b);
		}

		while(1) {
			if(!a) return b;
			if(!b) return a;

			T remainder = a%b;
			a = b;
			b = remainder;
		}
#endif
	}

	/*!
		@returns The least common multiple of @c a and @c b computed indirectly via the greatest
		common divisor.
	*/
	template<class T> T least_common_multiple(T a, T b) {
		if(a == b) return a;

		T gcd = greatest_common_divisor<T>(a, b);
		return (a / gcd) * (b / gcd) * gcd;
	}
}

#endif /* Factors_hpp */
