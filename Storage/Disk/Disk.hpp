//
//  Disk.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 10/07/2016.
//  Copyright 2016 Thomas Harte. All rights reserved.
//

#pragma once

#include <map>
#include <memory>
#include <mutex>
#include <set>

#include "Storage/Storage.hpp"
#include "Storage/Disk/Track/Track.hpp"
#include "Concurrency/AsyncTaskQueue.hpp"

namespace Storage::Disk {

/*!
	Models a flopy disk.
*/
class Disk {
public:
	virtual ~Disk() = default;

	/*!
		@returns the number of discrete positions that this disk uses to model its complete surface area.

		This is not necessarily a track count. There is no implicit guarantee that every position will
		return a distinct track, or, e.g. if the media is holeless, will return any track at all.
	*/
	virtual HeadPosition maximum_head_position() const = 0;

	/*!
		@returns the number of heads (and, therefore, impliedly surfaces) available on this disk.
	*/
	virtual int head_count() const = 0;

	/*!
		@returns the @c Track at @c position underneath @c head if there are any detectable events there;
		returns @c nullptr otherwise.
	*/
	virtual Track *track_at_position(Track::Address) const = 0;

	/*!
		Replaces the Track at position @c position underneath @c head with @c track. Ignored if this disk is read-only.
	*/
	virtual void set_track_at_position(Track::Address, const std::shared_ptr<Track> &) = 0;

	/*!
		Provides a hint that no further tracks are likely to be written for a while.
	*/
	virtual void flush_tracks() = 0;

	/*!
		@returns whether the disk image is read only. Defaults to @c true if not overridden.
	*/
	virtual bool is_read_only() const = 0;

	/*!
		@returns @c true if the tracks at the two addresses are different. @c false if they are the same track.
			This can avoid some degree of work when disk images offer sub-head-position precision.
	*/
	virtual bool tracks_differ(Track::Address, Track::Address) const = 0;

	/*!
		@returns @c true if the file named by the string is what underlies this disk image; @c false otherwise.
	*/
	virtual bool represents(const std::string &) const = 0;

	/*!
		@returns @c true if this disk has been written to at any point; @c false otherwise.
	*/
	virtual bool has_written() const = 0;
};

}
