//
//  DiskController.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 14/07/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#ifndef Storage_Disk_Controller_hpp
#define Storage_Disk_Controller_hpp

#include "Drive.hpp"
#include "DigitalPhaseLockedLoop.hpp"
#include "PCMSegment.hpp"
#include "../TimedEventLoop.hpp"

namespace Storage {
namespace Disk {

/*!
	Provides the shell for emulating a disk controller — something that is connected to a disk drive and uses a
	phase locked loop ('PLL') to decode a bit stream from the surface of the disk.

	Partly abstract; it is expected that subclasses will provide methods to deal with receiving a newly-recognised
	bit from the PLL and with crossing the index hole.

	TODO: communication of head size and permissible stepping extents, appropriate simulation of gain.
*/
class Controller: public DigitalPhaseLockedLoop::Delegate, public TimedEventLoop {
	protected:
		/*!
			Constructs a @c DiskDrive that will be run at @c clock_rate and runs its PLL at @c clock_rate*clock_rate_multiplier,
			spinning inserted disks at @c revolutions_per_minute.
		*/
		Controller(unsigned int clock_rate, unsigned int clock_rate_multiplier, unsigned int revolutions_per_minute);

		/*!
			Communicates to the PLL the expected length of a bit as a fraction of a second.
		*/
		void set_expected_bit_length(Time bit_length);

		/*!
			Advances the drive by @c number_of_cycles cycles.
		*/
		void run_for_cycles(int number_of_cycles);

		/*!
			Sets the current drive.
		*/
		void set_drive(std::shared_ptr<Drive> drive);
		void invalidate_track();

		/*!
			Enables or disables the disk motor.
		*/
		void set_motor_on(bool motor_on);

		/*!
			@returns @c true if the motor is on; @c false otherwise.
		*/
		bool get_motor_on();

		/*!
			Begins write mode, initiating a PCM sampled region of data. During write mode, no events will
			be reported. Bits should be written via @c write_bit. They will be written with the length set
			via @c set_expected_bit_length.
		*/
		void begin_writing();

		/*!
			Writes the bit @c value as the next in the PCM stream initiated by @c begin_writing.
		*/
		void write_bit(bool value);

		/*!
			Ends write mode, switching back to read mode. The drive will return to posting events based
			on the recorded track.
		*/
		void end_writing();

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

		// to satisfy DigitalPhaseLockedLoop::Delegate
		void digital_phase_locked_loop_output_bit(int value);

		bool get_is_track_zero();
		void step(int direction);
		virtual bool get_drive_is_ready();

	private:
		Time bit_length_;
		unsigned int clock_rate_;
		unsigned int clock_rate_multiplier_;
		Time rotational_multiplier_;

		std::shared_ptr<DigitalPhaseLockedLoop> pll_;
		std::shared_ptr<Drive> drive_;
		std::shared_ptr<Track> track_;
		unsigned int cycles_since_index_hole_, cycles_since_event_;

		inline void get_next_event();
		Track::Event current_event_;
		Time time_into_track_of_last_event_;
		bool motor_is_on_;

		bool is_reading_;
		bool track_is_dirty_;
		PCMSegment write_segment_;
		Time write_start_time_;

		void setup_track();
		Time get_time_into_track();
};

}
}

#endif /* DiskDrive_hpp */
