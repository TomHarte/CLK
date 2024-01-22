//
//  DiskIIDrive.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 20/11/2020.
//  Copyright © 2020 Thomas Harte. All rights reserved.
//

#pragma once

#include "IWM.hpp"

namespace Apple::Disk {

class DiskIIDrive: public IWMDrive {
	public:
		DiskIIDrive(int input_clock_rate);

	private:
		void set_enabled(bool) final;
		void set_control_lines(int) final;
		bool read() final;

		int stepper_mask_ = 0;
		int stepper_position_ = 0;
};

}
