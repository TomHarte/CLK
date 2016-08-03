//
//  Factors.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 29/07/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#include "Factors.hpp"

unsigned int NumberTheory::greatest_common_divisor(unsigned int a, unsigned int b)
{
	if(a < b)
	{
		unsigned int swap = b;
		b = a;
		a = swap;
	}

	while(1) {
		if(!a) return b;
		if(!b) return a;

		unsigned int remainder = a%b;
		a = b;
		b = remainder;
	}
}

unsigned int NumberTheory::least_common_multiple(unsigned int a, unsigned int b)
{
	if(a == b) return a;

	unsigned int gcd = greatest_common_divisor(a, b);
	return (a / gcd) * (b / gcd);
}
