//
//  DiskImage.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 21/09/2017.
//  Copyright © 2017 Thomas Harte. All rights reserved.
//

#ifndef DiskImage_hpp
#define DiskImage_hpp

#include <map>
#include <memory>

#include "../Disk.hpp"
#include "../Track/Track.hpp"

namespace Storage {
namespace Disk {

/*!
	Models a disk image as a collection of tracks, plus a range of possible track positions.

	The intention is not that tracks necessarily be evenly spaced; a head_position_count of 3 wih track
	A appearing in positions 0 and 1, and track B appearing in position 2 is an appropriate use of this API
	if it matches the media.
*/
class DiskImage {
	public:
		virtual ~DiskImage() {}

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
		virtual std::shared_ptr<Track> get_track_at_position(unsigned int head, unsigned int position) = 0;

		/*!
			Replaces the Track at position @c position underneath @c head with @c track. Ignored if this disk is read-only.
			Subclasses that are not read-only should use the protected methods @c get_is_modified and, optionally,
			@c get_modified_track_at_position to query for changes when closing.
		*/
		virtual void set_track_at_position(unsigned int head, unsigned int position, const std::shared_ptr<Track> &track) {}

		/*!
			@returns whether the disk image is read only. Defaults to @c true if not overridden.
		*/
		virtual bool get_is_read_only() { return true; }
};

class DiskImageHolderBase: public Disk {
	protected:
		int get_id_for_track_at_position(unsigned int head, unsigned int position);
		std::map<int, std::shared_ptr<Track>> cached_tracks_;
		std::unique_ptr<Concurrency::AsyncTaskQueue> update_queue_;
};

/*!
	Provides a wrapper that wraps a DiskImage to make it into a Disk, providing caching and,
	thereby, an intermediate store for modified tracks so that mutable disk images can either
	update on the fly or perform a block update on closure, as appropriate.
*/
template <typename T> class DiskImageHolder: public DiskImageHolderBase {
	public:
		template <typename... Ts> DiskImageHolder(Ts&&... args) :
			disk_image_(args...) {}
		~DiskImageHolder();

		unsigned int get_head_position_count();
		unsigned int get_head_count();
		std::shared_ptr<Track> get_track_at_position(unsigned int head, unsigned int position);
		void set_track_at_position(unsigned int head, unsigned int position, const std::shared_ptr<Track> &track);
		bool get_is_read_only();

	private:
		T disk_image_;
};

#include "DiskImageImplementation.hpp"

}
}

#endif /* DiskImage_hpp */
