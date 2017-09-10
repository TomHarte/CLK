//
//  Drive.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 25/09/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#ifndef Drive_hpp
#define Drive_hpp

#include "Disk.hpp"
#include "PCMSegment.hpp"
#include "PCMPatchedTrack.hpp"

#include "../TimedEventLoop.hpp"
#include "../../ClockReceiver/Sleeper.hpp"

#include <memory>

namespace Storage {
namespace Disk {

class Drive: public Sleeper, public TimedEventLoop {
	public:
		Drive(unsigned int input_clock_rate, int revolutions_per_minute);

		/*!
			Replaces whatever is in the drive with @c disk.
		*/
		void set_disk(const std::shared_ptr<Disk> &disk);

		/*!
			Replaces whatever is in the drive with a disk that contains endless copies of @c track.
		*/
		void set_disk_with_track(const std::shared_ptr<Track> &track);

		/*!
			@returns @c true if a disk is currently inserted; @c false otherwise.
		*/
		bool has_disk();

		/*!
			@returns @c true if the drive head is currently at track zero; @c false otherwise.
		*/
		bool get_is_track_zero();

		/*!
			Steps the disk head the specified number of tracks. Positive numbers step inwards (i.e. away from track 0),
			negative numbers step outwards (i.e. towards track 0).
		*/
		void step(int direction);

		/*!
			Sets the current read head.
		*/
		void set_head(unsigned int head);

		/*!
			@returns @c true if the inserted disk is read-only or no disk is inserted; @c false otherwise.
		*/
		bool get_is_read_only();

		/*!
			@returns the track underneath the current head at the location now stepped to.
		*/
		std::shared_ptr<Track> get_track();

		/*!
			Attempts to set @c track as the track underneath the current head at the location now stepped to.
		*/
		void set_track(const std::shared_ptr<Track> &track);

		/*!
			@returns @c true if the drive is ready; @c false otherwise.
		*/
		bool get_is_ready();

		/*!
			Sets whether the disk motor is on.
		*/
		void set_motor_on(bool);

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
			Advances the drive by @c number_of_cycles cycles.
		*/
		void run_for(const Cycles cycles);

		/*!
			Provides a mechanism to receive track events as they occur, including the synthetic
			event of "you told me to output the following data, and I've done that now".
		*/
		struct EventDelegate {
			/// Informs the delegate that @c event has been reached.
			virtual void process_event(const Track::Event &event) = 0;

			/*!
				If the drive is in write mode, announces that all queued bits have now been written.
				If the controller provides further bits now then there will be no gap in written data.
			*/
			virtual void process_write_completed() {}
		};

		/// Sets the current event delegate.
		void set_event_delegate(EventDelegate *);

		// As per Sleeper.
		bool is_sleeping();

	private:
		// Drives [usually] contain an entire disk; from that a certain track
		// will be currently under the head.
		std::shared_ptr<Disk> disk_;
		std::shared_ptr<Track> track_;
		bool has_disk_ = false;

		// Contains the multiplier that converts between track-relative lengths
		// to real-time lengths — so it's the reciprocal of rotation speed.
		Time rotational_multiplier_;

		// A count of time since the index hole was last seen. Which is used to
		// determine how far the drive is into a full rotation when switching to
		// a new track.
		int cycles_since_index_hole_ = 0;

		// A record of head position and active head.
		int head_position_ = 0;
		unsigned int head_ = 0;

		// Motor control state.
		bool motor_is_on_ = false;

		// If the drive is not currently reading then it is writing. While writing
		// it can optionally be told to clamp to the index hole.
		bool is_reading_;
		bool clamp_writing_to_index_hole_;

		// If writing is occurring then the drive will be accumulating a write segment,
		// for addition to a patched track.
		std::shared_ptr<PCMPatchedTrack> patched_track_;
		PCMSegment write_segment_;
		Time write_start_time_;

		// Maintains appropriate counting to know when to indicate that writing
		// is complete.
		Time cycles_until_bits_written_;
		Time cycles_per_bit_;

		// TimedEventLoop call-ins and state.
		void process_next_event();
		void get_next_event(const Time &duration_already_passed);
		Track::Event current_event_;

		// Helper for track changes.
		Time get_time_into_track();

		// The target (if any) for track events.
		EventDelegate *event_delegate_ = nullptr;
};


}
}

#endif /* Drive_hpp */
