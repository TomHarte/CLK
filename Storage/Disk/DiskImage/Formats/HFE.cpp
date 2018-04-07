//
//  HFE.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 17/08/2017.
//  Copyright © 2017 Thomas Harte. All rights reserved.
//

#include "HFE.hpp"

#include "../../Track/PCMTrack.hpp"
#include "../../Track/TrackSerialiser.hpp"
#include "../../../Data/BitReverse.hpp"

using namespace Storage::Disk;

HFE::HFE(const std::string &file_name) :
		file_(file_name) {
	if(!file_.check_signature("HXCPICFE")) throw ErrorNotHFE;

	if(file_.get8()) throw ErrorNotHFE;
	track_count_ = file_.get8();
	head_count_ = file_.get8();

	file_.seek(7, SEEK_CUR);
	track_list_offset_ = static_cast<long>(file_.get16le()) << 9;
}

HFE::~HFE() {
}

int HFE::get_head_position_count() {
	return track_count_;
}

int HFE::get_head_count() {
	return head_count_;
}

/*!
	Seeks to the beginning of the track at @c position underneath @c head,
	returning its length in bytes.

	To read the track, start from the current file position, read 256 bytes,
	skip 256 bytes, read 256 bytes, skip 256 bytes, etc.
*/
uint16_t HFE::seek_track(Track::Address address) {
	// Get track position and length from the lookup table; data is then always interleaved
	// based on an assumption of two heads.
	file_.seek(track_list_offset_ + address.position * 4, SEEK_SET);

	long track_offset = static_cast<long>(file_.get16le()) << 9;
	uint16_t track_length = file_.get16le();

	file_.seek(track_offset, SEEK_SET);
	if(address.head) file_.seek(256, SEEK_CUR);

	return track_length / 2;
}

std::shared_ptr<Track> HFE::get_track_at_position(Track::Address address) {
	PCMSegment segment;
	{
		std::lock_guard<std::mutex> lock_guard(file_.get_file_access_mutex());
		uint16_t track_length = seek_track(address);

		segment.data.resize(track_length);
		segment.number_of_bits = track_length * 8;

		uint16_t c = 0;
		while(c < track_length) {
			uint16_t length = static_cast<uint16_t>(std::min(256, track_length - c));
			file_.read(&segment.data[c], length);
			c += length;
			file_.seek(256, SEEK_CUR);
		}
	}

	// Flip bytes; HFE's preference is that the least-significant bit
	// is serialised first, but PCMTrack posts the most-significant first.
	Storage::Data::BitReverse::reverse(segment.data);

	std::shared_ptr<Track> track(new PCMTrack(segment));
	return track;
}

void HFE::set_tracks(const std::map<Track::Address, std::shared_ptr<Track>> &tracks) {
	for(auto &track : tracks) {
		std::unique_lock<std::mutex> lock_guard(file_.get_file_access_mutex());
		uint16_t track_length = seek_track(track.first);
		lock_guard.unlock();

		PCMSegment segment = Storage::Disk::track_serialisation(*track.second, Storage::Time(1, track_length * 8));
		Storage::Data::BitReverse::reverse(segment.data);
		uint16_t data_length = std::min(static_cast<uint16_t>(segment.data.size()), track_length);

		lock_guard.lock();
		seek_track(track.first);

		uint16_t c = 0;
		while(c < data_length) {
			uint16_t length = static_cast<uint16_t>(std::min(256, data_length - c));
			file_.write(&segment.data[c], length);
			c += length;
			file_.seek(256, SEEK_CUR);
		}
		lock_guard.unlock();
	}
}

bool HFE::get_is_read_only() {
	return file_.get_is_known_read_only();
}
