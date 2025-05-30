//
//  MSA.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 03/10/2019.
//  Copyright © 2019 Thomas Harte. All rights reserved.
//

#include "MSA.hpp"

#include "Storage/Disk/DiskImage/Formats/Utility/ImplicitSectors.hpp"

#include <cassert>

using namespace Storage::Disk;

MSA::MSA(const std::string &file_name) :
	file_(file_name) {
	const auto signature = file_.get_be<uint16_t>();
	if(signature != 0x0e0f) throw Error::InvalidFormat;

	sectors_per_track_ = file_.get_be<uint16_t>();
	sides_ = 1 + file_.get_be<uint16_t>();
	starting_track_ = file_.get_be<uint16_t>();
	ending_track_ = file_.get_be<uint16_t>();

	// Create the uncompressed track list.
	while(true) {
		const auto data_length = file_.get_be<uint16_t>();
		if(file_.eof()) break;

		if(data_length == sectors_per_track_ * 512) {
			// This is an uncompressed track.
			uncompressed_tracks_.push_back(file_.read(data_length));
		} else {
#ifndef NDEBUG
			const auto start_of_track = file_.tell();
#endif
			// This is an RLE-compressed track.
			std::vector<uint8_t> track;
			track.reserve(sectors_per_track_ * 512);
			uint16_t pointer = 0;
			while(pointer < data_length) {
				const auto byte = file_.get();

				// Compression scheme: if the byte E5 is encountered, an RLE run follows.
				// An RLE run is encoded as the byte to repeat plus a 16-bit repeat count.
				if(byte != 0xe5) {
					track.push_back(byte);
					++pointer;
					continue;
				}

				pointer += 4;
				if(pointer > data_length) break;

				const auto value = file_.get();
				auto count = file_.get_be<uint16_t>();
				while(count--) {
					track.push_back(value);
				}
			}

#ifndef NDEBUG
			assert(file_.tell() - start_of_track == pointer);
#endif

			if(pointer != data_length || track.size() != sectors_per_track_ * 512)
				throw Error::InvalidFormat;
			uncompressed_tracks_.push_back(std::move(track));
		}
	}

	if(uncompressed_tracks_.size() != size_t((ending_track_ - starting_track_ + 1)*sides_))
		throw Error::InvalidFormat;
}

std::unique_ptr<Track> MSA::track_at_position(Track::Address address) const {
	if(address.head >= sides_) return nullptr;

	const auto position = address.position.as_int();
	if(position < starting_track_) return nullptr;
	if(position > ending_track_) return nullptr;

	const auto &track = uncompressed_tracks_[size_t(position - starting_track_) * size_t(sides_) + size_t(address.head)];
	assert(!track.empty());
	return track_for_sectors(track.data(), sectors_per_track_, uint8_t(position), uint8_t(address.head), 1, 2, Storage::Encodings::MFM::Density::Double);
}

HeadPosition MSA::maximum_head_position() const {
	return HeadPosition(ending_track_ + 1);
}

int MSA::head_count() const {
	return sides_;
}

bool MSA::represents(const std::string &name) const {
	return name == file_.name();
}
