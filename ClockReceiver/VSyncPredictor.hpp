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
#include <cassert>
#include <cmath>
#include <cstdio>

namespace Time {

/*!
	For platforms that provide no avenue into vsync tracking other than block-until-sync,
	this class tracks: (i) how long frame draw takes; (ii) the apparent frame period; and
	(iii) optionally, timer jitter; in order to suggest when you should next start drawing.
*/
class VSyncPredictor {
	public:
		/*!
			Announces to the predictor that the work of producing an output frame has begun.
		*/
		void begin_redraw() {
			redraw_begin_time_ = nanos_now();
		}

		/*!
			Announces to the predictor that the work of producing an output frame has ended;
			the predictor will use the amount of time between each begin/end pair to modify
			its expectations as to how long it takes to draw a frame.
		*/
		void end_redraw() {
			redraw_period_.post(nanos_now() - redraw_begin_time_);
		}

		/*!
			Informs the predictor that a block-on-vsync has just ended, i.e. that the moment this
			machine calls retrace is now. The predictor uses these notifications to estimate output
			frame rate.
		*/
		void announce_vsync() {
			const auto now = nanos_now();

			if(last_vsync_) {
				last_vsync_ += frame_duration_;
				vsync_jitter_.post(last_vsync_ - now);
				last_vsync_ = (last_vsync_ + now) >> 1;
			} else {
				last_vsync_ = now;
			}
		}

		/*!
			Sets the frame rate for the target display.
		*/
		void set_frame_rate(float rate) {
			frame_duration_ = Nanos(1'000'000'000.0f / rate);
		}

		/*!
			@returns The time this class currently believes a whole frame occupies.
		*/
		Time::Nanos frame_duration() {
			return frame_duration_;
		}

		/*!
			Adds a record of how much jitter was experienced in scheduling; these values will be
			factored into the @c suggested_draw_time if supplied.

			A positive number means the timer occurred late. A negative number means it occurred early.
		*/
		void add_timer_jitter(Time::Nanos jitter) {
			timer_jitter_.post(jitter);
		}

		/*!
			Announces to the vsync predictor that output is now paused. This ends frame period
			calculations until the next announce_vsync() restarts frame-length counting.
		*/
		void pause() {
			last_vsync_ = 0;
		}

		/*!
			@return The time at which redrawing should begin, given the predicted frame period, how
			long it appears to take to draw a frame and how much jitter there is in scheduling
			(if those figures are being supplied).
		*/
		Nanos suggested_draw_time() {
			const auto mean = redraw_period_.mean() + timer_jitter_.mean() + vsync_jitter_.mean();
			const auto variance = redraw_period_.variance() + timer_jitter_.variance() + vsync_jitter_.variance();

			// Permit three standard deviations from the mean, to cover 99.9% of cases.
			const auto period = mean + Nanos(3.0f * sqrt(float(variance)));

			return last_vsync_ + frame_duration_ - period;
		}

	private:
		class VarianceCollector {
			public:
				VarianceCollector(Time::Nanos default_value) {
					sum_ = default_value * 128;
					for(int c = 0; c < 128; ++c) {
						history_[c] = default_value;
					}
				}

				void post(Time::Nanos value) {
					sum_ -= history_[write_pointer_];
					sum_ += value;
					history_[write_pointer_] = value;
					write_pointer_ = (write_pointer_ + 1) & 127;
				}

				Time::Nanos mean() {
					return sum_ / 128;
				}

				Time::Nanos variance() {
					// I haven't yet come up with a better solution that calculating this
					// in whole every time, given the way that the mean mutates.
					Time::Nanos variance = 0;
					for(int c = 0; c < 128; ++c) {
						const auto difference = ((history_[c] * 128) - sum_) / 128;
						variance += (difference * difference);
					}
					return variance / 128;
				}

			private:
				Time::Nanos sum_;
				Time::Nanos history_[128];
				size_t write_pointer_ = 0;
		};

		Nanos redraw_begin_time_ = 0;
		Nanos last_vsync_ = 0;
		Nanos frame_duration_ = 1'000'000'000 / 60;

		VarianceCollector vsync_jitter_{0};
		VarianceCollector redraw_period_{1'000'000'000 / 60};	// A less convincing first guess.
		VarianceCollector timer_jitter_{0};						// Seed at 0 in case this feature isn't used by the owner.
};

}

#endif /* VSyncPredictor_hpp */
