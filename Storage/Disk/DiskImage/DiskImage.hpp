//
//  DiskImage.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 21/09/2017.
//  Copyright 2017 Thomas Harte. All rights reserved.
//

#pragma once

#include <map>
#include <memory>

#include "../Disk.hpp"
#include "../Track/Track.hpp"
#include "../../TargetPlatforms.hpp"

namespace Storage::Disk {

enum class Error {
	InvalidFormat = -2,
	UnknownVersion = -3
};

/*!
	Models a disk image as a collection of tracks, plus a range of possible track positions.

	The intention is not that tracks necessarily be evenly spaced; a head_position_count of 3 wih track
	A appearing in positions 0 and 1, and track B appearing in position 2 is an appropriate use of this API
	if it matches the media.
*/
class DiskImage {
public:
	virtual ~DiskImage() = default;

	/*!
		@returns the distance at which there stops being any further content.

		This is not necessarily a track count. There is no implicit guarantee that every position will
		return a distinct track, or, e.g. if the media is holeless, will return any track at all.
	*/
	virtual HeadPosition get_maximum_head_position() = 0;

	/*!
		@returns the number of heads (and, therefore, impliedly surfaces) available on this disk.
	*/
	virtual int get_head_count() { return 1; }

	/*!
		@returns the @c Track at @c position underneath @c head if there are any detectable events there;
		returns @c nullptr otherwise.
	*/
	virtual std::unique_ptr<Track> track_at_position(Track::Address address) = 0;

	/*!
		Replaces the Tracks indicated by the map, that maps from physical address to track content.
	*/
	virtual void set_tracks(const std::map<Track::Address, std::unique_ptr<Track>> &) {}

	/*!
		Communicates that it is likely to be a while before any more tracks are written.
	*/
	virtual void flush_tracks() {}

	/*!
		@returns whether the disk image is read only. Defaults to @c true if not overridden.
	*/
	virtual bool get_is_read_only() { return true; }

	/*!
		@returns @c true if the tracks at the two addresses are different. @c false if they are the same track.
			This can avoid some degree of work when disk images offer sub-head-position precision.
	*/
	virtual bool tracks_differ(Track::Address lhs, Track::Address rhs) { return lhs != rhs; }

	/*!
		Maps from an address to its canonical form; this provides a means for images that duplicate the same
		track at different addresses to declare as much.
	*/
	virtual Track::Address canonical_address(Track::Address address) { return address; }
};

class DiskImageHolderBase: public Disk {
	protected:
		std::set<Track::Address> unwritten_tracks_;
		std::map<Track::Address, std::shared_ptr<Track>> cached_tracks_;
		std::unique_ptr<Concurrency::AsyncTaskQueue<true>> update_queue_;
};

/*!
	Provides a wrapper that wraps a DiskImage to make it into a Disk, providing caching and,
	thereby, an intermediate store for modified tracks so that mutable disk images can either
	update on the fly or perform a block update on closure, as appropriate.

	Implements TargetPlatform::TypeDistinguisher to return either no information whatsoever, if
	the underlying image doesn't implement TypeDistinguisher, or else to pass the call along.
*/
template <typename T> class DiskImageHolder: public DiskImageHolderBase, public TargetPlatform::Distinguisher {
public:
	template <typename... Ts> DiskImageHolder(Ts&&... args) :
		disk_image_(args...) {}
	~DiskImageHolder();

	HeadPosition get_maximum_head_position();
	int get_head_count();
	Track *track_at_position(Track::Address address);
	void set_track_at_position(Track::Address address, const std::shared_ptr<Track> &track);
	void flush_tracks();
	bool get_is_read_only();
	bool tracks_differ(Track::Address lhs, Track::Address rhs);

private:
	T disk_image_;

	TargetPlatform::Type target_platforms() final {
		if constexpr (std::is_base_of<TargetPlatform::Distinguisher, T>::value) {
			return static_cast<TargetPlatform::Distinguisher *>(&disk_image_)->target_platforms();
		} else {
			return TargetPlatform::Type(~0);
		}
	}
};

#include "DiskImageImplementation.hpp"

}
