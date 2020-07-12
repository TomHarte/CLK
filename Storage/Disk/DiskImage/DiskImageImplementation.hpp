//
//  DiskImageImplementation.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 22/09/2017.
//  Copyright 2017 Thomas Harte. All rights reserved.
//

template <typename T> HeadPosition DiskImageHolder<T>::get_maximum_head_position() {
	return disk_image_.get_maximum_head_position();
}

template <typename T> int DiskImageHolder<T>::get_head_count() {
	return disk_image_.get_head_count();
}

template <typename T> bool DiskImageHolder<T>::get_is_read_only() {
	return disk_image_.get_is_read_only();
}

template <typename T> void DiskImageHolder<T>::flush_tracks() {
	if(!unwritten_tracks_.empty()) {
		if(!update_queue_) update_queue_ = std::make_unique<Concurrency::AsyncTaskQueue>();

		using TrackMap = std::map<Track::Address, std::shared_ptr<Track>>;
		std::shared_ptr<TrackMap> track_copies(new TrackMap);
		for(const auto &address : unwritten_tracks_) {
			track_copies->insert(std::make_pair(address, std::shared_ptr<Track>(cached_tracks_[address]->clone())));
		}
		unwritten_tracks_.clear();

		update_queue_->enqueue([this, track_copies]() {
			disk_image_.set_tracks(*track_copies);
		});
	}
}

template <typename T> void DiskImageHolder<T>::set_track_at_position(Track::Address address, const std::shared_ptr<Track> &track) {
	if(disk_image_.get_is_read_only()) return;

	unwritten_tracks_.insert(address);
	cached_tracks_[address] = track;
}

template <typename T> std::shared_ptr<Track> DiskImageHolder<T>::get_track_at_position(Track::Address address) {
	if(address.head >= get_head_count()) return nullptr;
	if(address.position >= get_maximum_head_position()) return nullptr;

	auto cached_track = cached_tracks_.find(address);
	if(cached_track != cached_tracks_.end()) return cached_track->second;

	std::shared_ptr<Track> track = disk_image_.get_track_at_position(address);
	if(!track) return nullptr;
	cached_tracks_[address] = track;
	return track;
}

template <typename T> DiskImageHolder<T>::~DiskImageHolder() {
	if(update_queue_) update_queue_->flush();
}
