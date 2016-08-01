//
//  CommodoreGCR.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 29/07/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#include "CommodoreGCR.hpp"

using namespace Storage;

Time Storage::Encodings::CommodoreGCR::length_of_a_bit_in_time_zone(unsigned int time_zone)
{
	Time duration;
	// the speed zone divides a 4Mhz clock by 13, 14, 15 or 16, with higher-numbered zones being faster (i.e. each bit taking less time)
	duration.length = 16 - time_zone;
	duration.clock_rate = 4000000;
	return duration;
}
