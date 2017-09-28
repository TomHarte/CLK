//
//  Track.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 22/09/2017.
//  Copyright © 2017 Thomas Harte. All rights reserved.
//

#ifndef Track_h
#define Track_h

#include "../../Storage.hpp"

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
			Describes the location of a track, implementing < to allow for use as a set key.
		*/
		struct Address {
			int head, position;

			bool operator < (const Address &rhs) const {
				return (head < rhs.head) || (position < rhs.position);
			}
		};

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

}
}

#endif /* Track_h */
