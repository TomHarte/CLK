//
//  6560.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 05/06/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#include "6560.hpp"

using namespace MOS;

MOS6560::MOS6560() :
	_crt(new Outputs::CRT::CRT(65, 1, Outputs::CRT::DisplayType::NTSC60, 1))
{
	_crt->set_rgb_sampling_function(
		"vec3 rgb_sample(usampler2D sampler, vec2 coordinate, vec2 icoordinate)"
		"{"
			"return vec3(1.0);"
		"}");
}

void MOS6560::set_register(int address, uint8_t value)
{
	printf("%02x -> %d\n", value, address);
}

uint16_t MOS6560::get_address()
{
	return 0;
}

void MOS6560::set_graphics_value(uint8_t value)
{
}
