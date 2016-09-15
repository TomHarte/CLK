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
#include "../TimedEventLoop.hpp"

namespace Storage {
namespace Disk {

/*!
	Provides the shell for emulating a disk drive — something that takes a disk and has a drive head
	that steps between tracks, using a phase locked loop ('PLL') to decode a bit stream from the surface of
	the disk.

	Partly abstract; it is expected that subclasses will provide methods to deal with receiving a newly-recognised
	bit from the PLL and with crossing the index hole.

	TODO: double sided disks, communication of head size and permissible stepping extents, appropriate
	simulation of gain.
*/
class Drive: public DigitalPhaseLockedLoop::Delegate, public TimedEventLoop {
	public:
		/*!
			Constructs a @c DiskDrive that will be run at @c clock_rate and runs its PLL at @c clock_rate*clock_rate_multiplier,
			spinning inserted disks at @c revolutions_per_minute.
		*/
		Drive(unsigned int clock_rate, unsigned int clock_rate_multiplier, unsigned int revolutions_per_minute);

		/*!
			Communicates to the PLL the expected length of a bit.
		*/
		void set_expected_bit_length(Time bit_length);

		/*!
			Inserts @c disk into the drive.
		*/
		void set_disk(std::shared_ptr<Disk> disk);

		/*!
			@returns @c true if a disk is currently inserted; @c false otherwise.
		*/
		bool has_disk();

		/*!
			Advances the drive by @c number_of_cycles cycles.
		*/
		void run_for_cycles(int number_of_cycles);

		/*!
			@returns @c true if the drive head is currently at track zero; @c false otherwise.
		*/
		bool get_is_track_zero();

		/*!
			Steps the disk head the specified number of tracks. Positive numbers step inwards, negative numbers
			step outwards.
		*/
		void step(int direction);

		/*!
			Enables or disables the disk motor.
		*/
		void set_motor_on(bool motor_on);

		// to satisfy DigitalPhaseLockedLoop::Delegate
		void digital_phase_locked_loop_output_bit(int value);

	protected:
		/*!
			Should be implemented by subclasses; communicates each bit that the PLL recognises, also specifying
			the amount of time since the index hole was last seen.
		*/
		virtual void process_input_bit(int value, unsigned int cycles_since_index_hole) = 0;

		/*!
			Should be implemented by subcalsses; communicates that the index hole has been reached.
		*/
		virtual void process_index_hole() = 0;

		// for TimedEventLoop
		virtual void process_next_event();

	private:
		Time _bit_length;
		unsigned int _clock_rate;
		unsigned int _clock_rate_multiplier;
		Time _rotational_multiplier;

		std::shared_ptr<DigitalPhaseLockedLoop> _pll;
		std::shared_ptr<Disk> _disk;
		std::shared_ptr<Track> _track;
		int _head_position;
		unsigned int _cycles_since_index_hole;
		void set_track(Time initial_offset);

		inline void get_next_event();
		Track::Event _current_event;
		Time _time_into_track;
};

}
}

#endif /* DiskDrive_hpp */
