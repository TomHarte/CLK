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
	unsigned int greatest_common_divisor(unsigned int a, unsigned int b);

	/*!
		@returns The least common multiple of @c a and @c b computed indirectly via Euclid's greatest
		common divisor algorithm.
	*/
	unsigned int least_common_multiple(unsigned int a, unsigned int b);
}

#endif /* Factors_hpp */
