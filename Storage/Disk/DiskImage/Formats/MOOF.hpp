//
//  MOOF.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 15/02/2026.
//  Copyright © 2026 Thomas Harte. All rights reserved.
//

#pragma once

#include "Storage/Disk/DiskImage/DiskImage.hpp"
#include "Storage/FileHolder.hpp"

namespace Storage::Disk {

class MOOF: public DiskImage {
public:
	MOOF(const std::string &file_name);

	HeadPosition maximum_head_position() const;
	int head_count() const;
	std::unique_ptr<Track> track_at_position(Track::Address) const;
	bool represents(const std::string &) const;

private:
	mutable Storage::FileHolder file_;
	std::vector<uint8_t> post_crc_contents_;

	uint8_t track_map_[160];
	uint8_t flux_map_[160];
	long tracks_offset_ = -1;

	struct Info {
		uint8_t version = 100;
		enum class DiskType {
			GCR400kb = 1,
			GCR800kb = 2,
			MFM = 3,
			Twiggy = 4,
		} disk_type;
		bool is_write_protected;
	} info_;

	struct TrackLocation {
		uint16_t starting_block;
		uint16_t block_count;
		uint32_t bit_count;
	};
	std::unique_ptr<Track> flux(TrackLocation) const;
	std::unique_ptr<Track> track(TrackLocation) const;
};

}
