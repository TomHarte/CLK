//
//  Disk.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 10/07/2016.
//  Copyright 2016 Thomas Harte. All rights reserved.
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

/*!
	Models a flopy disk.
*/
class Disk {
	public:
		virtual ~Disk() {}

		/*!
			@returns the number of discrete positions that this disk uses to model its complete surface area.

			This is not necessarily a track count. There is no implicit guarantee that every position will
			return a distinct track, or, e.g. if the media is holeless, will return any track at all.
		*/
		virtual HeadPosition get_maximum_head_position() = 0;

		/*!
			@returns the number of heads (and, therefore, impliedly surfaces) available on this disk.
		*/
		virtual int get_head_count() = 0;

		/*!
			@returns the @c Track at @c position underneath @c head if there are any detectable events there;
			returns @c nullptr otherwise.
		*/
		virtual std::shared_ptr<Track> get_track_at_position(Track::Address address) = 0;

		/*!
			Replaces the Track at position @c position underneath @c head with @c track. Ignored if this disk is read-only.
		*/
		virtual void set_track_at_position(Track::Address address, const std::shared_ptr<Track> &track) = 0;

		/*!
			Provides a hint that no further tracks are likely to be written for a while.
		*/
		virtual void flush_tracks() = 0;

		/*!
			@returns whether the disk image is read only. Defaults to @c true if not overridden.
		*/
		virtual bool get_is_read_only() = 0;
};

}
}

#endif /* Disk_hpp */
