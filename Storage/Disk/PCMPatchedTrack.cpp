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
	const Time zero(0);
	const Time one(1);
	periods_.emplace_back(zero, one, zero, nullptr);
	active_period_ = periods_.begin();
	underlying_track_->seek_to(zero);
}

void PCMPatchedTrack::add_segment(const Time &start_time, const PCMSegment &segment)
{
	std::shared_ptr<PCMSegmentEventSource> event_source(new PCMSegmentEventSource(segment));

	Time zero(0);
	Time end_time = start_time + event_source->get_length();
	Period insertion_period(start_time, end_time, zero, event_source);

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
	active_period_ = periods_.begin();
	while(active_period_->start_time > current_time_) active_period_++;
}

void PCMPatchedTrack::insert_period(const Period &period)
{
	// find the existing period that the new period starts in
	std::vector<Period>::iterator start_period = periods_.begin();
	while(start_period->start_time >= period.end_time) start_period++;

	// find the existing period that the new period end in
	std::vector<Period>::iterator end_period = start_period;
	while(end_period->end_time < period.end_time) end_period++;

	// perform a division if called for
	if(start_period == end_period)
	{
		Period right_period = *start_period;

		Time adjustment = period.end_time - right_period.start_time;
		right_period.start_time += adjustment;
		right_period.segment_start_time += adjustment;

		start_period->end_time = period.start_time;
		periods_.insert(start_period + 1, period);
		periods_.insert(start_period + 2, right_period);
	}
	else
	{
		// perform a left chop on the thing at the start and a right chop on the thing at the end
		start_period->end_time = period.start_time;

		Time adjustment = period.end_time - end_period->start_time;
		end_period->start_time += adjustment;
		end_period->segment_start_time += adjustment;

		// remove anything in between
		periods_.erase(start_period + 1, end_period - 1);

		// insert the new period
		periods_.insert(start_period + 1, period);
	}
}

Track::Event PCMPatchedTrack::get_next_event()
{
	const Time one(1);
	const Time zero(0);
	Time extra_time(0);
	Time period_error(0);

	while(1)
	{
		// get the next event from the current active period
		Track::Event event;
		if(active_period_->event_source) event = active_period_->event_source->get_next_event();
		else event = underlying_track_->get_next_event();

		// see what time that gets us to. If it's still within the current period, return the found event
		Time event_time = current_time_ + event.length;
		if(event_time < active_period_->end_time)
		{
			current_time_ = event_time;
			event.length += extra_time - period_error;
			return event;
		}

		// otherwise move time back to the end of the outgoing period, accumulating the error into
		// extra_time, and advance the extra period
		extra_time += (active_period_->end_time - current_time_);
		current_time_ = active_period_->end_time;
		active_period_++;

		// test for having reached the end of the track
		if(active_period_ == periods_.end())
		{
			// if this is the end of the track then jump the active pointer back to the beginning
			// of the list of periods and reset current_time_ to zero
			active_period_ = periods_.begin();
			if(active_period_->event_source) active_period_->event_source->reset();
			else underlying_track_->seek_to(zero);
			current_time_ = zero;

			// then return an index hole that is the aggregation of accumulated extra_time away
			event.type = Storage::Disk::Track::Event::IndexHole;
			event.length = extra_time;
			return event;
		}
		else
		{
			// if this is not the end of the track then move to the next period and note how much will need
			// to be subtracted if an event is found here
			if(active_period_->event_source) period_error = active_period_->segment_start_time - active_period_->event_source->seek_to(active_period_->segment_start_time);
			else period_error = current_time_ - underlying_track_->seek_to(current_time_);
		}
	}
}

Storage::Time PCMPatchedTrack::seek_to(const Time &time_since_index_hole)
{
	// start at the beginning and continue while segments start after the time sought
	active_period_ = periods_.begin();
	while(active_period_->start_time > time_since_index_hole) active_period_++;

	// allow whatever storage represents the period found to perform its seek
	if(active_period_->event_source)
		return active_period_->event_source->seek_to(time_since_index_hole - active_period_->start_time) + active_period_->start_time;
	else
		return underlying_track_->seek_to(time_since_index_hole);
}
