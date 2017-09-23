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
#include "Track/Track.hpp"
#include "../../Concurrency/AsyncTaskQueue.hpp"

namespace Storage {
namespace Disk {

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
