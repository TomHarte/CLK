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
#include "DigitalPhaseLockedLoop.hpp"

namespace Storage {

class DiskDrive: public DigitalPhaseLockedLoop::Delegate {
	public:
		DiskDrive(unsigned int clock_rate, unsigned int revolutions_per_minute);

		void set_expected_bit_length(Time bit_length);

		void set_disk(std::shared_ptr<Disk> disk);
		bool has_disk();

		void run_for_cycles(unsigned int number_of_cycles);

		bool get_is_track_zero();
		void step(int direction);
		void set_motor_on(bool motor_on);

		// to satisfy DigitalPhaseLockedLoop::Delegate
		void digital_phase_locked_loop_output_bit(int value);

	protected:
		virtual void process_input_bit(int value, unsigned int cycles_since_index_hole) = 0;
		virtual void process_index_hole() = 0;

	private:
		Time _bit_length;
		unsigned int _clock_rate;
		unsigned int _revolutions_per_minute;
};

}

#endif /* DiskDrive_hpp */
