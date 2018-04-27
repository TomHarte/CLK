//
//  NIB.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 21/04/2018.
//  Copyright © 2018 Thomas Harte. All rights reserved.
//

#include "NIB.hpp"

#include "../../Track/PCMTrack.hpp"
#include "../../Encodings/AppleGCR.hpp"

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
		throw ErrorNotNIB;
	}

	// TODO: all other validation. I.e. does this look like a GCR disk?
}

int NIB::get_head_position_count() {
	return number_of_tracks * 4;
}

std::shared_ptr<::Storage::Disk::Track> NIB::get_track_at_position(::Storage::Disk::Track::Address address) {
	// NIBs contain data for even-numbered tracks underneath a single head only.
	if(address.head) return nullptr;

	const long file_track = static_cast<long>(address.position >> 2);
	file_.seek(file_track * track_length, SEEK_SET);
	std::vector<uint8_t> track_data = file_.read(track_length);

	// NIB files leave sync bytes implicit and make no guarantees
	// about overall track positioning. So the approach taken here
	// is to look for the epilogue sequence (which concludes all Apple
	// tracks and headers), then treat all following FFs as a sync
	// region, then switch back to ordinary behaviour as soon as a
	// non-FF appears.
	std::vector<Storage::Disk::PCMSegment> segments;

	std::size_t start_index = 0;
	std::set<size_t> sync_starts;

	// Establish where syncs start by finding instances of 0xd5 0xaa and then regressing
	// from each along all preceding FFs.
	for(size_t index = 0; index < track_data.size(); ++index) {
		if(track_data[index] == 0xd5 && track_data[(index+1)%track_data.size()] == 0xaa) {
			size_t start = index - 1;
			size_t length = 0;
			while(track_data[start] == 0xff) {
				start = (start + track_data.size() - 1) % track_data.size();
				++length;
			}

			if(length >= 5) {
				sync_starts.insert((start + 1) % track_data.size());
				if(start > index)
					start_index = start;
			}
		}
	}

	if(start_index)
		segments.push_back(Encodings::AppleGCR::six_and_two_sync(static_cast<int>(start_index)));

	std::size_t index = start_index;
	for(const auto &location: sync_starts) {
		// Write from index to sync_start.
		PCMSegment data_segment;
		data_segment.data.insert(
			data_segment.data.end(),
			track_data.begin() + static_cast<off_t>(index),
			track_data.begin() + static_cast<off_t>(location));
		data_segment.number_of_bits = static_cast<unsigned int>(data_segment.data.size() * 8);
		segments.push_back(std::move(data_segment));

		// Add a sync from sync_start to end of 0xffs.
		if(location == track_length-1) break;

		index = location;
		while(index < track_length && track_data[index] == 0xff)
			++index;
		segments.push_back(Encodings::AppleGCR::six_and_two_sync(static_cast<int>(index - location)));
	}

	return std::shared_ptr<PCMTrack>(new PCMTrack(segments));
}
