//
//  Drive.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 25/09/2016.
//  Copyright 2016 Thomas Harte. All rights reserved.
//

#ifndef Drive_hpp
#define Drive_hpp

#include "Disk.hpp"
#include "Track/PCMSegment.hpp"
#include "Track/PCMTrack.hpp"

#include "../TimedEventLoop.hpp"
#include "../../Activity/Observer.hpp"
#include "../../ClockReceiver/ClockingHintSource.hpp"

#include <memory>

namespace Storage {
namespace Disk {

class Drive: public ClockingHint::Source, public TimedEventLoop {
	public:
		Drive(unsigned int input_clock_rate, int revolutions_per_minute, int number_of_heads);
		Drive(unsigned int input_clock_rate, int number_of_heads);
		~Drive();

		/*!
			Replaces whatever is in the drive with @c disk.
		*/
		void set_disk(const std::shared_ptr<Disk> &disk);

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
		void step(HeadPosition offset);

		/*!
			Sets the current read head.
		*/
		void set_head(int head);

		/*!
			Gets the head count for this disk.
		*/
		int get_head_count();

		/*!
			@returns @c true if the inserted disk is read-only or no disk is inserted; @c false otherwise.
		*/
		bool get_is_read_only();

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
		void begin_writing(Time bit_length, bool clamp_to_index_hole);

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

		struct Event {
			Track::Event::Type type;
			float length = 0.0f;
		} current_event_;

		/*!
			Provides a mechanism to receive track events as they occur, including the synthetic
			event of "you told me to output the following data, and I've done that now".
		*/
		struct EventDelegate {
			/// Informs the delegate that @c event has been reached.
			virtual void process_event(const Event &event) = 0;

			/*!
				If the drive is in write mode, announces that all queued bits have now been written.
				If the controller provides further bits now then there will be no gap in written data.
			*/
			virtual void process_write_completed() {}

			/// Informs the delegate of the passing of @c cycles.
			virtual void advance(const Cycles cycles) {}
		};

		/// Sets the current event delegate.
		void set_event_delegate(EventDelegate *);

		// As per Sleeper.
		ClockingHint::Preference preferred_clocking() override;

		/// Adds an activity observer; it'll be notified of disk activity.
		/// The caller can specify whether to add an LED based on disk motor.
		void set_activity_observer(Activity::Observer *observer, const std::string &name, bool add_motor_led);

		/*!
			Attempts to step to the specified offset and returns the track there if one exists; an uninitialised
			track otherwise.

			This is unambiguously **NOT A REALISTIC DRIVE FUNCTION**; real drives cannot step to a given offset.
			So it is **NOT FOR HARDWARE EMULATION USAGE**.

			It's for the benefit of user-optional fast-loading mechanisms **ONLY**.
		*/
		std::shared_ptr<Track> step_to(HeadPosition offset);

		/*!
			Alters the rotational velocity of this drive.
		*/
		void set_rotation_speed(float revolutions_per_minute);

		/*!
			@returns the current value of the tachometer pulse offered by some drives.
		*/
		bool get_tachometer();

	protected:
		/*!
			Announces the result of a step.
		*/
		virtual void did_step(HeadPosition to_position) {}

		/*!
			@returns the current rotation of the disk, a float in the half-open range
				0.0 (the index hole) to 1.0 (back to the index hole, a whole rotation later).
		*/
		float get_rotation();

	private:
		// Drives contain an entire disk; from that a certain track
		// will be currently under the head.
		std::shared_ptr<Disk> disk_;
		std::shared_ptr<Track> track_;
		bool has_disk_ = false;

		// Contains the multiplier that converts between track-relative lengths
		// to real-time lengths. So it's the reciprocal of rotation speed.
		float rotational_multiplier_;

		// A count of time since the index hole was last seen. Which is used to
		// determine how far the drive is into a full rotation when switching to
		// a new track.
		int cycles_since_index_hole_ = 0;

		// A record of head position and active head.
		HeadPosition head_position_;
		int head_ = 0;
		int available_heads_ = 0;

		// Motor control state.
		bool motor_is_on_ = false;

		// If the drive is not currently reading then it is writing. While writing
		// it can optionally be told to clamp to the index hole.
		bool is_reading_ = true;
		bool clamp_writing_to_index_hole_ = false;

		// If writing is occurring then the drive will be accumulating a write segment,
		// for addition to a (high-resolution) PCM track.
		std::shared_ptr<PCMTrack> patched_track_;
		PCMSegment write_segment_;
		Time write_start_time_;

		// Indicates progress towards drive ready state.
		int ready_index_count_ = 0;

		// Maintains appropriate counting to know when to indicate that writing
		// is complete.
		Time cycles_until_bits_written_;
		Time cycles_per_bit_;

		// TimedEventLoop call-ins and state.
		void process_next_event() override;
		void get_next_event(float duration_already_passed);
		void advance(const Cycles cycles) override;

		// Helper for track changes.
		float get_time_into_track();

		// The target (if any) for track events.
		EventDelegate *event_delegate_ = nullptr;

		/*!
			@returns the track underneath the current head at the location now stepped to.
		*/
		std::shared_ptr<Track> get_track();

		/*!
			Attempts to set @c track as the track underneath the current head at the location now stepped to.
		*/
		void set_track(const std::shared_ptr<Track> &track);

		void setup_track();
		void invalidate_track();

		// Activity observer description.
		Activity::Observer *observer_ = nullptr;
		std::string drive_name_;
		bool announce_motor_led_ = false;

		// A rotating random data source.
		uint64_t random_source_;
		float random_interval_;
};


}
}

#endif /* Drive_hpp */
