//
//  CRT.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 19/07/2015.
//  Copyright © 2015 Thomas Harte. All rights reserved.
//

#include "CRT.hpp"

using namespace Outputs;

CRT::CRT(int cycles_per_line)
{
	_horizontalOffset = 0.0f;
	_verticalOffset = 0.0f;
}

void CRT::output_sync(int number_of_cycles)
{
	// horizontal sync is edge triggered; vertical is integrated
	_syncCapacitorChargeLevel += number_of_cycles;
}

void CRT::output_level(int number_of_cycles, uint8_t *level, std::string type)
{
	_syncCapacitorChargeLevel -= number_of_cycles;
}

void CRT::output_data(int number_of_cycles, uint8_t *data, std::string type)
{
	_syncCapacitorChargeLevel -= number_of_cycles;
}
