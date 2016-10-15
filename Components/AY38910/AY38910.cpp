//
//  AY-3-8910.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 14/10/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#include "AY38910.hpp"

using namespace GI;

AY38910::AY38910()
{
}

void AY38910::get_samples(unsigned int number_of_samples, int16_t *target)
{
}

void AY38910::skip_samples(unsigned int number_of_samples)
{
}

void AY38910::select_register(uint8_t r)
{
	printf("sel %d\n", r);
}

void AY38910::set_register_value(uint8_t value)
{
	printf("val %d\n", value);
}

uint8_t AY38910::get_register_value()
{
	return 0;
}
