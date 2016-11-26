//
//  Disk.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 10/07/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#ifndef Disk_hpp
#define Disk_hpp

#include <memory>
#include <map>
#include "../Storage.hpp"

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
		virtual Time seek_to(Time time_since_index_hole) = 0;
};

/*!
	Models a disk as a collection of tracks, providing a range of possible track positions and allowing
	a point sampling of the track beneath any of those positions (if any).

	The intention is not that tracks necessarily be evenly spaced; a head_position_count of 3 wih track
	A appearing in positions 0 and 1, and track B appearing in position 2 is an appropriate use of this API
	if it matches the media.

	The track returned is point sampled only; if a particular disk drive has a sufficiently large head to
	pick up multiple tracks at once then the drive responsible for asking for multiple tracks and for
	merging the results.
*/
class Disk {
	public:

		/*!
			@returns the number of discrete positions that this disk uses to model its complete surface area.

			This is not necessarily a track count. There is no implicit guarantee that every position will
			return a distinct track, or — e.g. if the media is holeless — will return any track at all.
		*/
		virtual unsigned int get_head_position_count() = 0;

		/*!
			@returns the number of heads (and, therefore, impliedly surfaces) available on this disk.
		*/
		virtual unsigned int get_head_count() { return 1; }

		/*!
			@returns the @c Track at @c position underneath @c head if there are any detectable events there;
			returns @c nullptr otherwise.
		*/
		std::shared_ptr<Track> get_track_at_position(unsigned int head, unsigned int position);

	protected:
		/*!
			Subclasses should implement this to return the @c Track at @c position underneath @c head. Returned tracks
			are cached internally so subclasses shouldn't attempt to build their own caches or worry about preparing
			for track accesses at file load time. Appropriate behaviour is to create them lazily, on demand.
		*/
		virtual std::shared_ptr<Track> get_uncached_track_at_position(unsigned int head, unsigned int position) = 0;

	private:
		std::map<int, std::shared_ptr<Track>> cached_tracks_;
};

}
}

#endif /* Disk_hpp */
