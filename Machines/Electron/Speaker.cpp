//
//  Speaker.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 03/12/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#include "Speaker.hpp"

using namespace Electron;

void Speaker::get_samples(unsigned int number_of_samples, int16_t *target)
{
	if(is_enabled_)
	{
		while(number_of_samples--)
		{
			*target = (int16_t)((counter_ / (divider_+1)) * 8192);
			target++;
			counter_ = (counter_ + 1) % ((divider_+1) * 2);
		}
	}
	else
	{
		memset(target, 0, sizeof(int16_t) * number_of_samples);
	}
}

void Speaker::skip_samples(unsigned int number_of_samples)
{
	counter_ = (counter_ + number_of_samples) % ((divider_+1) * 2);
}

void Speaker::set_divider(uint8_t divider)
{
	enqueue([=]() {
		divider_ = divider * 32 / clock_rate_divider;
	});
}

void Speaker::set_is_enabled(bool is_enabled)
{
	enqueue([=]() {
		is_enabled_ = is_enabled;
		counter_ = 0;
	});
}
