//
//  DiskImageImplementation.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 22/09/2017.
//  Copyright © 2017 Thomas Harte. All rights reserved.
//

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
