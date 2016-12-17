//
//  PCMPatchedTrack.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 15/12/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#include "PCMPatchedTrack.hpp"

using namespace Storage::Disk;

void PCMPatchedTrack::add_segment(const Time &start_position, const PCMSegment &segment)
{
	patches_.emplace_back(start_position, segment);
}
