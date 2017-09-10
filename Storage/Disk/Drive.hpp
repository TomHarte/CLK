//
//  Drive.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 25/09/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#ifndef Drive_hpp
#define Drive_hpp

#include <memory>

#include "Disk.hpp"
#include "../../ClockReceiver/Sleeper.hpp"
#include "../TimedEventLoop.hpp"

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
			Advances the drive by @c number_of_cycles cycles.
		*/
		void run_for(const Cycles cycles);

		/*!
			Provides a mechanism to receive track events as they occur.
		*/
		struct EventDelegate {
			virtual void process_event(const Track::Event &event) = 0;
		};

		/// Sets the current event delegate.
		void set_event_delegate(EventDelegate *);

		// As per Sleeper.
		bool is_sleeping();

	private:
		std::shared_ptr<Track> track_;
		std::shared_ptr<Disk> disk_;
		int cycles_since_index_hole_ = 0;
		Time rotational_multiplier_;

		bool has_disk_ = false;

		int head_position_ = 0;
		unsigned int head_ = 0;

		void process_next_event();
		void get_next_event(const Time &duration_already_passed);
		Track::Event current_event_;
		bool motor_is_on_ = false;

		Time get_time_into_track();
		EventDelegate *event_delegate_ = nullptr;
};


}
}

#endif /* Drive_hpp */
