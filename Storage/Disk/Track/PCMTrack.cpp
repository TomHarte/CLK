//
//  PCMTrack.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 10/07/2016.
//  Copyright 2016 Thomas Harte. All rights reserved.
//

#include "PCMTrack.hpp"
#include "../../../NumberTheory/Factors.hpp"
#include "../../../Outputs/Log.hpp"

using namespace Storage::Disk;

PCMTrack::PCMTrack() : segment_pointer_(0) {}

PCMTrack::PCMTrack(const std::vector<PCMSegment> &segments) : PCMTrack() {
	// sum total length of all segments
	Time total_length;
	for(const auto &segment : segments) {
		total_length += segment.length_of_a_bit * static_cast<unsigned int>(segment.data.size());
	}
	total_length.simplify();

	// each segment is then some proportion of the total; for them all to sum to 1 they'll
	// need to be adjusted to be
	for(const auto &segment : segments) {
		Time original_length_of_segment = segment.length_of_a_bit * static_cast<unsigned int>(segment.data.size());
		Time proportion_of_whole = original_length_of_segment / total_length;
		proportion_of_whole.simplify();
		PCMSegment length_adjusted_segment = segment;
		length_adjusted_segment.length_of_a_bit = proportion_of_whole / static_cast<unsigned int>(segment.data.size());
		length_adjusted_segment.length_of_a_bit.simplify();
		segment_event_sources_.emplace_back(length_adjusted_segment);
	}
}

PCMTrack::PCMTrack(const PCMSegment &segment) : PCMTrack() {
	// a single segment necessarily fills the track
	PCMSegment length_adjusted_segment = segment;
	length_adjusted_segment.length_of_a_bit.length = 1;
	length_adjusted_segment.length_of_a_bit.clock_rate = static_cast<unsigned int>(segment.data.size());
	segment_event_sources_.emplace_back(std::move(length_adjusted_segment));
}

PCMTrack::PCMTrack(const PCMTrack &original) : PCMTrack() {
	segment_event_sources_ = original.segment_event_sources_;
}

PCMTrack::PCMTrack(unsigned int bits_per_track) : PCMTrack() {
	PCMSegment segment;
	segment.length_of_a_bit.length = 1;
	segment.length_of_a_bit.clock_rate = bits_per_track;
	segment.data.resize(bits_per_track);
	segment_event_sources_.emplace_back(segment);
}

PCMTrack *PCMTrack::resampled_clone(Track *original, size_t bits_per_track) {
	PCMTrack *pcm_original = dynamic_cast<PCMTrack *>(original);
	if(pcm_original) {
		return pcm_original->resampled_clone(bits_per_track);
	}

	ERROR("NOT IMPLEMENTED: resampling non-PCMTracks");
	return nullptr;
}

bool PCMTrack::is_resampled_clone() {
	return is_resampled_clone_;
}

Track *PCMTrack::clone() const {
	return new PCMTrack(*this);
}

PCMTrack *PCMTrack::resampled_clone(size_t bits_per_track) {
	// Create an empty track.
	PCMTrack *const new_track = new PCMTrack(static_cast<unsigned int>(bits_per_track));

	// Plot all segments from this track onto the destination.
	Time start_time;
	for(const auto &event_source: segment_event_sources_) {
		const PCMSegment &source = event_source.segment();
		new_track->add_segment(start_time, source, true);
		start_time += source.length();
	}

	new_track->is_resampled_clone_ = true;
	return new_track;
}

Track::Event PCMTrack::get_next_event() {
	// ask the current segment for a new event
	Track::Event event = segment_event_sources_[segment_pointer_].get_next_event();

	// if it was a flux transition, that's code for end-of-segment, so dig deeper
	if(event.type == Track::Event::IndexHole) {
		// multiple segments may be crossed, so start summing lengths in case the net
		// effect is an index hole
		Time total_length = event.length;

		// continue until somewhere no returning an index hole
		while(event.type == Track::Event::IndexHole) {
			// advance to the [start of] the next segment
			segment_pointer_ = (segment_pointer_ + 1) % segment_event_sources_.size();
			segment_event_sources_[segment_pointer_].reset();

			// if this is all the way back to the start, that's a genuine index hole,
			// so set the summed length and return
			if(!segment_pointer_) {
				return event;
			}

			// otherwise get the next event (if it's not another index hole, the loop will end momentarily),
			// summing in any prior accumulated time
			event = segment_event_sources_[segment_pointer_].get_next_event();
			total_length += event.length;
			event.length = total_length;
		}
	}

	return event;
}

Storage::Time PCMTrack::seek_to(const Time &time_since_index_hole) {
	// initial condition: no time yet accumulated, the whole thing requested yet to navigate
	Storage::Time accumulated_time;
	Storage::Time time_left_to_seek = time_since_index_hole;

	// search from the first segment
	segment_pointer_ = 0;
	do {
		// if this segment extends beyond the amount of time left to seek, trust it to complete
		// the seek
		Storage::Time segment_time = segment_event_sources_[segment_pointer_].get_length();
		if(segment_time > time_left_to_seek) {
			return accumulated_time + segment_event_sources_[segment_pointer_].seek_to(time_left_to_seek);
		}

		// otherwise swallow this segment, updating the time left to seek and time so far accumulated
		time_left_to_seek -= segment_time;
		accumulated_time += segment_time;
		segment_pointer_ = (segment_pointer_ + 1) % segment_event_sources_.size();
	} while(segment_pointer_);

	// if all segments have now been swallowed, the closest we can get is the very end of
	// the list of segments
	return accumulated_time;
}

void PCMTrack::add_segment(const Time &start_time, const PCMSegment &segment, bool clamp_to_index_hole) {
	// Get a reference to the destination.
	PCMSegment &destination = segment_event_sources_.front().segment();

	// Determine the range to fill on the target segment.
	const Time end_time = start_time + segment.length();
	const size_t start_bit = start_time.length * destination.data.size() / start_time.clock_rate;
	const size_t end_bit = end_time.length * destination.data.size() / end_time.clock_rate;
	const size_t target_width = end_bit - start_bit;
	const size_t half_offset = target_width / (2 * segment.data.size());

	if(clamp_to_index_hole || end_bit <= destination.data.size()) {
		// If clamping is applied, just write a single segment, from the start_bit to whichever is
		// closer of the end of track and the end_bit.
		const size_t selected_end_bit = std::min(end_bit, destination.data.size());

		// Reset the destination.
		std::fill(destination.data.begin() + off_t(start_bit), destination.data.begin() + off_t(selected_end_bit), false);

		// Step through the source data from start to finish, stopping early if it goes out of bounds.
		for(size_t bit = 0; bit < segment.data.size(); ++bit) {
			if(segment.data[bit]) {
				const size_t output_bit = start_bit + half_offset + (bit * target_width) / segment.data.size();
				if(output_bit >= destination.data.size()) return;
				destination.data[output_bit] = true;
			}
		}
	} else {
		// Clamping is not enabled, so the supplied segment loops over the index hole, arbitrarily many times.
		// So work backwards unless or until the original start position is reached, then stop.

		// This definitely runs over the index hole; check whether the whole track needs clearing, or whether
		// a centre segment is untouched.
		if(target_width >= destination.data.size()) {
			std::fill(destination.data.begin(), destination.data.end(), false);
		} else {
			std::fill(destination.data.begin(), destination.data.begin() + off_t(end_bit % destination.data.size()), false);
			std::fill(destination.data.begin() + off_t(start_bit), destination.data.end(), false);
		}

		// Run backwards from final bit back to first, stopping early if overlapping the beginning.
		for(off_t bit = off_t(segment.data.size()-1); bit >= 0; --bit) {
			// Store flux transitions only; non-transitions can be ignored.
			if(segment.data[size_t(bit)]) {
				// Map to the proper output destination; stop if now potentially overwriting where we began.
				const size_t output_bit = start_bit + half_offset + (size_t(bit) * target_width) / segment.data.size();
				if(output_bit < end_bit - destination.data.size()) return;

				// Store.
				destination.data[output_bit % destination.data.size()] = true;
			}
		}
	}
}
