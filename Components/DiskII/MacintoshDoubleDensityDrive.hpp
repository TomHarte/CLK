//
//  MacintoshDoubleDensityDrive.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 10/07/2019.
//  Copyright © 2019 Thomas Harte. All rights reserved.
//

#ifndef MacintoshDoubleDensityDrive_hpp
#define MacintoshDoubleDensityDrive_hpp

#include "IWM.hpp"

namespace Apple {
namespace Macintosh {

class DoubleDensityDrive: public IWMDrive {
	public:
		DoubleDensityDrive(int input_clock_rate, bool is_800k);

		/*!
			@returns @c true if this is an 800kb drive; @c false otherwise.
		*/
		bool is_800k() {
			return is_800k_;
		}

		/*!
			Sets the current rotation speed of this drive only if it is a 400kb drive.
			800kb drives select their own rotation speed based on head position,
			and ignore this input.
		*/
		void set_rotation_speed(float revolutions_per_minute);

		void set_enabled(bool) override;
		void set_control_lines(int) override;
		bool read() override;

	private:
		// To receive the proper notifications from Storage::Disk::Drive.
		void did_step(Storage::Disk::HeadPosition to_position) override;
		void did_set_disk() override;

		const bool is_800k_;
		bool has_new_disk_ = false;
		int control_state_ = 0;
		int step_direction_ = 1;
};

}
}

#endif /* MacintoshDoubleDensityDrive_hpp */
