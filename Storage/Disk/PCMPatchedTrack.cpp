//
//  PCMPatchedTrack.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 15/12/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#include "PCMPatchedTrack.hpp"

using namespace Storage::Disk;

PCMPatchedTrack::PCMPatchedTrack(Track &underlying_track) :
	underlying_track_(underlying_track),
	active_patch_((size_t)-1)
{}

void PCMPatchedTrack::add_segment(const Time &start_position, const PCMSegment &segment)
{
	patches_.emplace_back(start_position, segment);
}

Track::Event PCMPatchedTrack::get_next_event()
{
//	if(active_patch_ == (size_t)-1)
	return underlying_track_.get_next_event();
}

Storage::Time PCMPatchedTrack::seek_to(const Time &time_since_index_hole)
{
	return underlying_track_.seek_to(time_since_index_hole);
}
