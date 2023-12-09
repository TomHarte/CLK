//
//  NIB.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 21/04/2018.
//  Copyright 2018 Thomas Harte. All rights reserved.
//

#include "NIB.hpp"

#include "../../Track/PCMTrack.hpp"
#include "../../Track/TrackSerialiser.hpp"
#include "../../Encodings/AppleGCR/Encoder.hpp"

#include "../../Encodings/AppleGCR/Encoder.hpp"
#include "../../Encodings/AppleGCR/SegmentParser.hpp"

#include <vector>

using namespace Storage::Disk;

namespace {

const long track_length = 6656;
const std::size_t number_of_tracks = 35;

}

NIB::NIB(const std::string &file_name) :
	file_(file_name) {
	// A NIB should be 35 tracks, each 6656 bytes long.
	if(file_.stats().st_size != track_length*number_of_tracks) {
		throw Error::InvalidFormat;
	}

	// A real NIB should have every single top bit set. Yes, 1/8th of the
	// file size is a complete waste. But it provides a hook for validation.
	while(true) {
		uint8_t next = file_.get8();
		if(file_.eof()) break;
		if(!(next & 0x80)) throw Error::InvalidFormat;
	}
}

HeadPosition NIB::get_maximum_head_position() {
	return HeadPosition(number_of_tracks);
}

bool NIB::get_is_read_only() {
	return file_.get_is_known_read_only();
}

long NIB::file_offset(Track::Address address) {
	return long(address.position.as_int()) * track_length;
}

std::shared_ptr<::Storage::Disk::Track> NIB::get_track_at_position(::Storage::Disk::Track::Address address) {
	// NIBs contain data for even-numbered tracks underneath a single head only.
	if(address.head) return nullptr;
	if(address.position.as_quarter() & 2) return nullptr;

	long offset = file_offset(address);
	std::vector<uint8_t> track_data;
	{
		std::lock_guard lock_guard(file_.get_file_access_mutex());
		if(cached_offset_ == offset && cached_track_) {
			return cached_track_;
		}
		file_.seek(offset, SEEK_SET);
		track_data = file_.read(track_length);
	}

	// NIB files leave sync bytes implicit and make no guarantees
	// about overall track positioning. The attempt works by locating
	// any single run of FF that is sufficiently long and marking the last
	// five as including slip bits.
	std::set<size_t> sync_locations;
	for(size_t index = 0; index < track_data.size(); ++index) {
		// Count the number of FFs starting from here.
		size_t length = 0;
		size_t end = index;
		while(track_data[end] == 0xff) {
			end = (end + 1) % track_data.size();
			++length;
		}

		// If that's at least five, regress and mark all as syncs.
		if(length >= 5) {
			for(int c = 0; c < 5; c++) {
				end = (end + track_data.size() - 1) % track_data.size();
				sync_locations.insert(end);
			}
			break;
		}
	}

	PCMSegment segment;
	std::size_t index = 0;
	while(index < track_data.size()) {
		// Deal with a run of sync values, if present.
		const auto sync_start = index;
		while(sync_locations.find(index) != sync_locations.end() && index < track_data.size()) {
			++index;
		}
		if(index != sync_start) {
			segment += Encodings::AppleGCR::six_and_two_sync(int(index - sync_start));
		}

		// Deal with regular data.
		const auto data_start = index;
		while(sync_locations.find(index) == sync_locations.end() && index < track_data.size()) {
			++index;
		}
		if(index != data_start) {
			std::vector<uint8_t> data_segment(
				track_data.begin() + ptrdiff_t(data_start),
				track_data.begin() + ptrdiff_t(index));
			segment += PCMSegment(data_segment);
		}
	}

	std::lock_guard lock_guard(file_.get_file_access_mutex());
	cached_offset_ = offset;
	cached_track_ = std::make_shared<PCMTrack>(segment);;
	return cached_track_;
}

void NIB::set_tracks(const std::map<Track::Address, std::shared_ptr<Track>> &tracks) {
	std::map<Track::Address, std::vector<uint8_t>> tracks_by_address;

	// Convert to a map from address to a vector of data that contains the NIB representation
	// of the track.
	for(const auto &pair: tracks) {
		// Grab the track bit stream.
		auto segment = Storage::Disk::track_serialisation(*pair.second, Storage::Time(1, 50000));

		// Process to eliminate all sync bits.
		std::vector<uint8_t> track;
		track.reserve(track_length);
		uint8_t shifter = 0;
		int bit_count = 0;
		size_t sync_location = 0, location = 0;
		for(const auto bit: segment.data) {
			shifter = uint8_t((shifter << 1) | (bit ? 1 : 0));
			++bit_count;
			++location;
			if(shifter & 0x80) {
				track.push_back(shifter);
				if(bit_count == 10) {
					sync_location = location;
				}
				shifter = 0;
				bit_count = 0;
			}
		}

		// Trim or pad out to track_length.
		if(track.size() > track_length) {
			track.resize(track_length);
		} else {
			while(track.size() < track_length) {
				std::vector<uint8_t> extra_data(size_t(track_length) - track.size(), 0xff);
				track.insert(track.begin() + ptrdiff_t(sync_location), extra_data.begin(), extra_data.end());
			}
		}

		tracks_by_address[pair.first] = std::move(track);
	}

	// Lock the file and spool out.
	std::lock_guard lock_guard(file_.get_file_access_mutex());
	for(const auto &track: tracks_by_address) {
		file_.seek(file_offset(track.first), SEEK_SET);
		file_.write(track.second);
	}
	cached_track_ = nullptr;	// Conservative, but safe.
}
