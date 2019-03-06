//
//  DisplayMetrics.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 04/03/2019.
//  Copyright © 2019 Thomas Harte. All rights reserved.
//

#include "DisplayMetrics.hpp"

#include <iostream>

using namespace Outputs::Display;

// MARK: Frame size estimation.

void Metrics::announce_event(ScanTarget::Event event) {
	switch(event) {
		case ScanTarget::Event::EndHorizontalRetrace:
			++lines_this_frame_;
		break;
		case ScanTarget::Event::EndVerticalRetrace:
			add_line_total(lines_this_frame_);
			lines_this_frame_ = 0;
		break;
		default: break;
	}
}

void Metrics::add_line_total(int total) {
//	std::cout << total << '\n';
}

float Metrics::lines_per_frame_estimate() {
	return 1.0f;	// TODO.
}

// MARK: GPU processing speed decisions.

void Metrics::announce_did_resize() {
	frames_missed_ = frames_hit_ = 0;
}

void Metrics::announce_draw_status(size_t lines, std::chrono::high_resolution_clock::duration duration, bool complete) {
	if(!complete) {
		++frames_missed_;
	} else {
		++frames_hit_;
	}

	// Don't allow the record of history to extend too far into the past.
	if(frames_hit_ + frames_missed_ > 200) {
		// Subtract from whichever wasn't just incremented, to ensure the
		// most recent information is more important than the historic stuff.
		if(!complete) {
			--frames_hit_;
		} else {
			--frames_missed_;
		}

		// Rebalance if either thing has gone negative.
		if(frames_hit_ < 0) {
			frames_missed_ += frames_hit_;
			frames_hit_ = 0;
		}
		if(frames_missed_ < 0) {
			frames_hit_ += frames_missed_;
			frames_missed_ = 0;
		}
	}
}

bool Metrics::should_lower_resolution() {
	// If less than 100 frames are on record, return no opinion; otherwise
	// suggest a lower resolution if more than 10 frames in the last 100-200
	// took too long to produce.
	if(frames_hit_ + frames_missed_ < 100) return false;
	return frames_missed_ > 10;
}
