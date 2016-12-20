//
//  PCMPatchedTrack.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 15/12/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#include "PCMPatchedTrack.hpp"

using namespace Storage::Disk;

PCMPatchedTrack::PCMPatchedTrack(std::shared_ptr<Track> underlying_track) :
	underlying_track_(underlying_track)
{
	Time zero(0);
	Time one(1);
	periods_.emplace_back(zero, one, zero, nullptr);
	active_period_ = &periods_.back();
}

void PCMPatchedTrack::add_segment(const Time &start_time, const PCMSegment &segment)
{
	event_sources_.emplace_back(segment);

	Time zero(0);
	Time end_time = start_time + event_sources_.back().get_length();
	Period insertion_period(start_time, end_time, zero, &event_sources_.back());

	// the new segment may wrap around, so divide it up into track-length parts if required
	Time one = Time(1);
	while(insertion_period.end_time > one)
	{
		Time next_end_time = insertion_period.end_time - one;
		insertion_period.end_time = one;
		insert_period(insertion_period);

		insertion_period.start_time = zero;
		insertion_period.end_time = next_end_time;
	}
	insert_period(insertion_period);

	// the vector may have been resized, potentially invalidating active_period_ even if
	// the thing it pointed to is still the same thing. So work it out afresh.
	for(auto period : periods_)
	{
		if(period.start_time <= current_time_ && period.end_time > current_time_)
		{
			active_period_ = &period;
			break;
		}
	}
}

void PCMPatchedTrack::insert_period(const Period &period)
{
	// find the existing period that the new period starts in
	size_t start_index = 0;
	while(periods_[start_index].start_time >= period.end_time) start_index++;

	// find the existing period that the new period end in
	size_t end_index = 0;
	while(periods_[end_index].end_time < period.end_time) end_index++;

	// perform a division if called for
	if(start_index == end_index)
	{
		Period right_period = periods_[start_index];

		Time adjustment = period.start_time - right_period.start_time;
		right_period.end_time += adjustment;
		right_period.segment_start_time += adjustment;

		periods_[start_index].end_time = period.start_time;
		periods_.insert(periods_.begin() + (int)start_index + 1, period);
		periods_.insert(periods_.begin() + (int)start_index + 2, right_period);
	}
	else
	{
		// perform a left chop on the thing at the start and a right chop on the thing at the end
		periods_[start_index].end_time = period.start_time;

		Time adjustment = period.start_time - periods_[end_index].start_time;
		periods_[end_index].end_time += adjustment;
		periods_[end_index].segment_start_time += adjustment;

		// remove anything in between
		periods_.erase(periods_.begin() + (int)start_index + 1, periods_.begin() + (int)end_index - 1);

		// insert the new period
		periods_.insert(periods_.begin() + (int)start_index + 1, period);
	}
}

Track::Event PCMPatchedTrack::get_next_event()
{
	while(1)
	{
		Track::Event event;
		if(active_period_->event_source) event = active_period_->event_source->get_next_event();
		else event = underlying_track_->get_next_event();

		current_time_ += event.length;
		if(current_time_ >= active_period_->end_time)
		{
			current_time_ -= active_period_->end_time;
			active_period_++;
//			if(active_period_
		}
	}
//		return active_period_->event_source->seek_to(zero);

	return underlying_track_->get_next_event();
}

Storage::Time PCMPatchedTrack::seek_to(const Time &time_since_index_hole)
{
	for(auto period : periods_)
	{
		if(period.start_time <= time_since_index_hole && period.end_time > time_since_index_hole)
		{
			active_period_ = &period;
			if(period.event_source)
				return period.event_source->seek_to(time_since_index_hole - period.start_time) + period.start_time;
			else
				return underlying_track_->seek_to(time_since_index_hole);
		}
	}

	// this should never be reached
	Time zero(0);
	active_period_ = &periods_[0];
	if(active_period_->event_source)
		return active_period_->event_source->seek_to(zero);
	else
		return underlying_track_->seek_to(zero);
}
