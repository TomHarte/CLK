//
//  DiskImageImplementation.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 22/09/2017.
//  Copyright 2017 Thomas Harte. All rights reserved.
//

template <typename T>
HeadPosition DiskImageHolder<T>::get_maximum_head_position() const {
	return disk_image_.get_maximum_head_position();
}

template <typename T>
int DiskImageHolder<T>::get_head_count() const {
	return disk_image_.get_head_count();
}

template <typename T>
bool DiskImageHolder<T>::get_is_read_only() const {
	return disk_image_.get_is_read_only();
}

template <typename T>
bool DiskImageHolder<T>::represents(const std::string &file) const {
	return false;	// TODO.
//	return disk_image_.represents(file);
}

template <typename T>
bool DiskImageHolder<T>::has_written() const {
	return has_written_;
}

template <typename T>
void DiskImageHolder<T>::flush_tracks() {
	if(!unwritten_tracks_.empty()) {
		if(!update_queue_) update_queue_ = std::make_unique<Concurrency::AsyncTaskQueue<true>>();

		using TrackMap = std::map<Track::Address, std::unique_ptr<Track>>;
		auto track_copies = std::make_shared<TrackMap>();
		for(const auto &address : unwritten_tracks_) {
			track_copies->insert({address, std::unique_ptr<Track>(cached_tracks_[address]->clone())});
		}
		unwritten_tracks_.clear();

		update_queue_->enqueue([this, track_copies]() {
			disk_image_.set_tracks(*track_copies);
		});
	}
}

template <typename T>
void DiskImageHolder<T>::set_track_at_position(Track::Address address, const std::shared_ptr<Track> &track) {
	if(disk_image_.get_is_read_only()) return;
	has_written_ = true;

	unwritten_tracks_.insert(address);
	cached_tracks_[address] = track;
}

template <typename T>
Track *DiskImageHolder<T>::track_at_position(Track::Address address) const {
	if(address.head >= get_head_count()) return nullptr;
	if(address.position >= get_maximum_head_position()) return nullptr;

	const auto canonical_address = disk_image_.canonical_address(address);
	auto cached_track = cached_tracks_.find(canonical_address);
	if(cached_track != cached_tracks_.end()) return cached_track->second.get();

	std::shared_ptr<Track> track = disk_image_.track_at_position(canonical_address);
	if(!track) return nullptr;
	cached_tracks_[canonical_address] = track;
	return track.get();
}

template <typename T>
DiskImageHolder<T>::~DiskImageHolder() {
	if(update_queue_) update_queue_->flush();
}

template <typename T>
bool DiskImageHolder<T>::tracks_differ(Track::Address lhs, Track::Address rhs) const {
	return disk_image_.tracks_differ(lhs, rhs);
}
