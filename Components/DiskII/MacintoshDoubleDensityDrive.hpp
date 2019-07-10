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

		void set_enabled(bool) override;
		void set_control_lines(int) override;
		bool read() override;
		void write(bool value) override;

	private:
		// To receive the proper notifications from Storage::Disk::Drive.
		void did_step(Storage::Disk::HeadPosition to_position) override;

		bool is_800k_;
		int control_state_;
		int step_direction_;
};

}
}

#endif /* MacintoshDoubleDensityDrive_hpp */
