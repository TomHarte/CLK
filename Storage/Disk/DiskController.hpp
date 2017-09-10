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
#include "PCMPatchedTrack.hpp"
#include "../TimedEventLoop.hpp"

#include "../../ClockReceiver/ClockReceiver.hpp"
#include "../../ClockReceiver/Sleeper.hpp"

namespace Storage {
namespace Disk {

/*!
	Provides the shell for emulating a disk controller — something that is connected to a disk drive and uses a
	phase locked loop ('PLL') to decode a bit stream from the surface of the disk.

	Partly abstract; it is expected that subclasses will provide methods to deal with receiving a newly-recognised
	bit from the PLL and with crossing the index hole.

	TODO: communication of head size and permissible stepping extents, appropriate simulation of gain.
*/
class Controller: public DigitalPhaseLockedLoop::Delegate, public TimedEventLoop, public Sleeper, public Sleeper::SleepObserver {
	protected:
		/*!
			Constructs a @c DiskDrive that will be run at @c clock_rate and runs its PLL at @c clock_rate*clock_rate_multiplier,
			spinning inserted disks at @c revolutions_per_minute.
		*/
		Controller(Cycles clock_rate, int clock_rate_multiplier, int revolutions_per_minute);

		/*!
			Communicates to the PLL the expected length of a bit as a fraction of a second.
		*/
		void set_expected_bit_length(Time bit_length);

		/*!
			Advances the drive by @c number_of_cycles cycles.
		*/
		void run_for(const Cycles cycles);

		/*!
			Sets the current drive.
		*/
		void set_drive(std::shared_ptr<Drive> drive);

		/*!
			Announces that the track the drive sees is about to change for a reason unknownt to the controller.
		*/
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
			Begins write mode, initiating a PCM sampled region of data. Bits should be written via
			@c write_bit. They will be written with the length set via @c set_expected_bit_length.
			It is acceptable to supply a backlog of bits. Flux transition events will not be reported
			while writing.

			@param clamp_to_index_hole If @c true then writing will automatically be truncated by
			the index hole. Writing will continue over the index hole otherwise.
		*/
		void begin_writing(bool clamp_to_index_hole);

		/*!
			Writes the bit @c value as the next in the PCM stream initiated by @c begin_writing.
		*/
		void write_bit(bool value);

		/*!
			Ends write mode, switching back to read mode. The drive will stop overwriting events.
		*/
		void end_writing();

		/*!
			Should be implemented by subclasses; communicates each bit that the PLL recognises, also specifying
			the amount of time since the index hole was last seen.
		*/
		virtual void process_input_bit(int value, unsigned int cycles_since_index_hole) = 0;

		/*!
			Should be implemented by subclasses; communicates that the index hole has been reached.
		*/
		virtual void process_index_hole() = 0;

		/*!
			Should be implemented by subclasses if they implement writing; communicates that
			all bits supplied to write_bit have now been written.
		*/
		virtual void process_write_completed();

		// for TimedEventLoop
		virtual void process_next_event();

		// to satisfy DigitalPhaseLockedLoop::Delegate
		void digital_phase_locked_loop_output_bit(int value);

		bool get_is_track_zero();
		void step(int direction);
		virtual bool get_drive_is_ready();
		bool get_drive_is_read_only();

		bool is_sleeping();

	private:
		Time bit_length_;
		int clock_rate_;
		int clock_rate_multiplier_;
		Time rotational_multiplier_;

		std::shared_ptr<DigitalPhaseLockedLoop> pll_;
		std::shared_ptr<Drive> drive_;
		std::shared_ptr<Track> track_;
		int cycles_since_index_hole_;

		inline void get_next_event(const Time &duration_already_passed);
		Track::Event current_event_;
		bool motor_is_on_;

		bool is_reading_;
		bool clamp_writing_to_index_hole_;
		std::shared_ptr<PCMPatchedTrack> patched_track_;
		PCMSegment write_segment_;
		Time write_start_time_;

		Time cycles_until_bits_written_;
		Time cycles_per_bit_;

		void setup_track();
		Time get_time_into_track();

		void set_component_is_sleeping(void *component, bool is_sleeping);
};

}
}

#endif /* DiskDrive_hpp */
