//
//  VSyncPredictor.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 14/06/2020.
//  Copyright © 2020 Thomas Harte. All rights reserved.
//

#ifndef VSyncPredictor_hpp
#define VSyncPredictor_hpp

#include "TimeTypes.hpp"

namespace Time {

/*!
	For platforms that provide no avenue into vsync tracking other than block-until-sync,
	this class tracks: (i) how long frame draw takes; and (ii) the apparent frame period; in order
	to suggest when you should next start drawing.
*/
class VSyncPredictor {
	public:
		void begin_redraw() {
			redraw_begin_time_ = nanos_now();
		}

		void end_redraw() {
			const auto redraw_length = nanos_now() - redraw_begin_time_;
			redraw_period_ =
				((redraw_period_ * 9) +
				((redraw_length) * 1)) / 10;
		}

		void announce_vsync() {
			const auto vsync_time = nanos_now();
			if(last_vsync_) {
				// Use an IIR to try to converge on frame times.
				vsync_period_ =
					((vsync_period_ * 9) +
					((vsync_time - last_vsync_) * 1)) / 10;
			}
			last_vsync_ = vsync_time;
		}

		void pause() {
			last_vsync_ = 0;
		}

		Nanos suggested_draw_time() {
			// TODO: this is a very simple version of how this calculation
			// should be made. It's tracking the average amount of time these
			// things take, therefore will often be wrong. Deviations need to
			// be accounted for.
			return last_vsync_ + vsync_period_ - redraw_period_;
		}

	private:
		Nanos redraw_begin_time_ = 0;
		Nanos last_vsync_ = 0;

		Nanos vsync_period_ = 1'000'000'000 / 60;	// Seems like a good first guess.
		Nanos redraw_period_ = 0;
};

}

#endif /* VSyncPredictor_hpp */
