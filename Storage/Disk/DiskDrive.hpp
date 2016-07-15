//
//  DiskDrive.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 14/07/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#ifndef DiskDrive_hpp
#define DiskDrive_hpp

#include "Disk.hpp"

namespace Storage {

class DiskDrive {
	public:
		DiskDrive(unsigned int clock_rate);

		void set_disk(std::shared_ptr<Disk> disk);
		bool has_disk();

		void run_for_cycles(unsigned int number_of_cycles);

		bool get_is_track_zero();
		void step(int direction);
		void set_motor_on(bool motor_on);

	protected:
		virtual void process_input_event(Track::Event event) = 0;
};

}

#endif /* DiskDrive_hpp */
