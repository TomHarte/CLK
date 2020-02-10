//
//  ScanSynchroniser.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 09/02/2020.
//  Copyright © 2020 Thomas Harte. All rights reserved.
//

#ifndef ScanSynchroniser_h
#define ScanSynchroniser_h

#include "../Outputs/ScanTarget.hpp"

#include <cmath>

namespace Time {

/*!
	Where an emulated machine is sufficiently close to a host machine's frame rate that a small nudge in
	its speed multiplier will bring it into frame synchronisation, the ScanSynchroniser provides a sequence of
	speed multipliers designed both to adjust the machine to the proper speed and, in a reasonable amount
	of time, to bring it into phase.
*/
class ScanSynchroniser {
	public:
		/*!
			@returns @c true if the emulated machine can be synchronised with the host frame output based on its
				current @c [scan]status and the host machine's @c frame_duration; @c false otherwise.
		*/
		bool can_synchronise(const Outputs::Display::ScanStatus &scan_status, double frame_duration) {
			ratio_ = 1.0;
			if(scan_status.field_duration_gradient < 0.00001) {
				// Check out the machine's current frame time.
				// If it's within 3% of a non-zero integer multiple of the
				// display rate, mark this time window to be split over the sync.
				ratio_ = frame_duration / scan_status.field_duration;
				const double integer_ratio = round(ratio_);
				if(integer_ratio > 0.0) {
					ratio_ /= integer_ratio;
					return ratio_ <= maximum_rate_adjustment && ratio_ >= 1.0 / maximum_rate_adjustment;
				}
			}
			return false;
		}

		/*!
			@returns The appropriate speed multiplier for the next frame based on the inputs previously supplied to @c can_synchronise.
				Results are undefined if @c can_synchroise returned @c false.
		*/
		double next_speed_multiplier(const Outputs::Display::ScanStatus &scan_status) {
			// The host versus emulated ratio is calculated based on the current perceived frame duration of the machine.
			// Either that number is exactly correct or it's already the result of some sort of low-pass filter. So there's
			// no benefit to second guessing it here — just take it to be correct.
			//
			// ... with one slight caveat, which is that it is desireable to adjust phase here, to align vertical sync points.
			// So the set speed multiplier may be adjusted slightly to aim for that.
			double speed_multiplier = 1.0 / ratio_;
			if(scan_status.current_position > 0.0) {
				if(scan_status.current_position < 0.5) speed_multiplier /= phase_adjustment_ratio;
				else speed_multiplier *= phase_adjustment_ratio;
			}
			speed_multiplier_ = (speed_multiplier_ * 0.95) + (speed_multiplier * 0.05);
			return speed_multiplier_;
		}

	private:
		static constexpr double maximum_rate_adjustment = 1.03;
		static constexpr double phase_adjustment_ratio = 1.005;

		// Managed local state.
		double speed_multiplier_ = 1.0;

		// Temporary storage to bridge the can_synchronise -> next_speed_multiplier gap.
		double ratio_ = 1.0;
};

}

#endif /* ScanSynchroniser_h */
