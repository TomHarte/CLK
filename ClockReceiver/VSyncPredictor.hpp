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
#include <cmath>

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
			redraw_period_.post(nanos_now() - redraw_begin_time_);
		}

		void announce_vsync() {
			const auto vsync_time = nanos_now();
			if(last_vsync_) {
				vsync_period_.post(vsync_time - last_vsync_);
			}
			last_vsync_ = vsync_time;
		}

		void pause() {
			last_vsync_ = 0;
		}

		Nanos suggested_draw_time() {
			const auto mean = (vsync_period_.mean() - redraw_period_.mean()) / 1;
			const auto variance = (vsync_period_.variance() + redraw_period_.variance()) / 1;

			// Permit three standard deviations from the mean, to cover 99.9% of cases.
			const auto period = mean + Nanos(3.0f * sqrt(float(variance)));

			return last_vsync_ + period;
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
						const auto difference = (history_[c] * 128) - sum_;
						variance += difference * difference;
					}
					return variance / (128 * 128 * 128);
				}

			private:
				Time::Nanos sum_;
				Time::Nanos history_[128];
				size_t write_pointer_ = 0;
		};

		Nanos redraw_begin_time_ = 0;
		Nanos last_vsync_ = 0;

		VarianceCollector vsync_period_{1'000'000'000 / 60};	// 60Hz: seems like a good first guess.
		VarianceCollector redraw_period_{1'000'000'000 / 60};	// A less convincing first guess.
};

}

#endif /* VSyncPredictor_hpp */
