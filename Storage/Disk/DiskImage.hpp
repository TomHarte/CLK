//
//  DiskImage.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 21/09/2017.
//  Copyright © 2017 Thomas Harte. All rights reserved.
//

#ifndef DiskImage_hpp
#define DiskImage_hpp

#include <cstdint>
#include <memory>
#include <vector>
#include "Disk.hpp"

namespace Storage {
namespace Disk {

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

class PatchingDiskImage {
	public:
		struct TrackUpdate {
			long file_offset;
			std::vector<uint8_t> data;
		};
};

class DiskImageHolderBase: public Disk {
	protected:
		int get_id_for_track_at_position(unsigned int head, unsigned int position);
		std::map<int, std::shared_ptr<Track>> cached_tracks_;

		std::unique_ptr<Concurrency::AsyncTaskQueue> update_queue_;
};

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

template <typename T> unsigned int DiskImageHolder<T>::get_head_position_count() {
	return disk_image_.get_head_position_count();
}

template <typename T> unsigned int DiskImageHolder<T>::get_head_count() {
	return disk_image_.get_head_count();
}

template <typename T> bool DiskImageHolder<T>::get_is_read_only() {
	return disk_image_.get_is_read_only();
}

template <typename T> void DiskImageHolder<T>::set_track_at_position(unsigned int head, unsigned int position, const std::shared_ptr<Track> &track) {
	if(disk_image_.get_is_read_only()) return;

	int address = get_id_for_track_at_position(head, position);
	cached_tracks_[address] = track;

	if(!update_queue_) update_queue_.reset(new Concurrency::AsyncTaskQueue);
	std::shared_ptr<Track> track_copy(track->clone());
	update_queue_->enqueue([this, head, position, track_copy] {
		disk_image_.set_track_at_position(head, position, track_copy);
	});
}

template <typename T> std::shared_ptr<Track> DiskImageHolder<T>::get_track_at_position(unsigned int head, unsigned int position) {
	if(head >= get_head_count()) return nullptr;
	if(position >= get_head_position_count()) return nullptr;

	int address = get_id_for_track_at_position(head, position);
	std::map<int, std::shared_ptr<Track>>::iterator cached_track = cached_tracks_.find(address);
	if(cached_track != cached_tracks_.end()) return cached_track->second;

	std::shared_ptr<Track> track = disk_image_.get_track_at_position(head, position);
	if(!track) return nullptr;
	cached_tracks_[address] = track;
	return track;
}

template <typename T> DiskImageHolder<T>::~DiskImageHolder() {
	if(update_queue_) update_queue_->flush();
}


}
}

#endif /* DiskImage_hpp */
