//
//  Factors.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 29/07/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#ifndef Factors_hpp
#define Factors_hpp

namespace NumberTheory {
	/*!
		@returns The greatest common divisor of @c a and @c b as computed by Euclid's algorithm.
	*/
	template<class T> T greatest_common_divisor(T a, T b) {
		if(a < b) {
			T swap = b;
			b = a;
			a = swap;
		}

		while(1) {
			if(!a) return b;
			if(!b) return a;

			T remainder = a%b;
			a = b;
			b = remainder;
		}
	}

	/*!
		@returns The least common multiple of @c a and @c b computed indirectly via Euclid's greatest
		common divisor algorithm.
	*/
	template<class T> T least_common_multiple(T a, T b) {
		if(a == b) return a;

		T gcd = greatest_common_divisor<T>(a, b);
		return (a / gcd) * (b / gcd) * gcd;
	}
}

#endif /* Factors_hpp */
