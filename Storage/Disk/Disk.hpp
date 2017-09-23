//
//  Disk.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 10/07/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#ifndef Disk_hpp
#define Disk_hpp

#include <map>
#include <memory>
#include <mutex>
#include <set>

#include "../Storage.hpp"
#include "../../Concurrency/AsyncTaskQueue.hpp"

namespace Storage {
namespace Disk {

/*!
	Models a single track on a disk as a series of events, each event being of arbitrary length
	and resulting in either a flux transition or the sensing of an index hole.
	
	Subclasses should implement @c get_next_event.
*/
class Track {
	public:
		/*!
			Describes a detectable track event — either a flux transition or the passing of the index hole,
			along with the length of time between the previous event and its occurance.

			The sum of all lengths of time across an entire track should be 1 — if an event is said to be
			1/3 away then that means 1/3 of a rotation.
		*/
		struct Event {
			enum {
				IndexHole, FluxTransition
			} type;
			Time length;
		};

		/*!
			@returns the next event that will be detected during rotation of this disk.
		*/
		virtual Event get_next_event() = 0;

		/*!
			Jumps to the event latest offset that is less than or equal to the input time.

			@returns the time jumped to.
		*/
		virtual Time seek_to(const Time &time_since_index_hole) = 0;

		/*!
			The virtual copy constructor pattern; returns a copy of the Track.
		*/
		virtual Track *clone() = 0;
};

class Disk {
	public:
		virtual ~Disk() {}

		/*!
			@returns the number of discrete positions that this disk uses to model its complete surface area.

			This is not necessarily a track count. There is no implicit guarantee that every position will
			return a distinct track, or — e.g. if the media is holeless — will return any track at all.
		*/
		virtual unsigned int get_head_position_count() = 0;

		/*!
			@returns the number of heads (and, therefore, impliedly surfaces) available on this disk.
		*/
		virtual unsigned int get_head_count() = 0;

		/*!
			@returns the @c Track at @c position underneath @c head if there are any detectable events there;
			returns @c nullptr otherwise.
		*/
		virtual std::shared_ptr<Track> get_track_at_position(unsigned int head, unsigned int position) = 0;

		/*!
			Replaces the Track at position @c position underneath @c head with @c track. Ignored if this disk is read-only.
			Subclasses that are not read-only should use the protected methods @c get_is_modified and, optionally,
			@c get_modified_track_at_position to query for changes when closing.
		*/
		virtual void set_track_at_position(unsigned int head, unsigned int position, const std::shared_ptr<Track> &track) = 0;

		/*!
			@returns whether the disk image is read only. Defaults to @c true if not overridden.
		*/
		virtual bool get_is_read_only() = 0;
};

}
}

#endif /* Disk_hpp */
