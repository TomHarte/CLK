//
//  IPF.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 25/12/2021.
//  Copyright © 2021 Thomas Harte. All rights reserved.
//

#pragma once

#include "Storage/Disk/DiskImage/DiskImage.hpp"
#include "Storage/Disk/Track/PCMTrack.hpp"
#include "Storage/FileHolder.hpp"
#include "Storage/TargetPlatforms.hpp"

#include <string>
#include <map>

namespace Storage::Disk {

/*!
	Provides a @c DiskImage containing an IPF, which is a mixed stream of raw flux windows and
	unencoded MFM sections along with gap records that can be used to record write splices, all
	of which is variably clocked (albeit not at flux transition resolution; as a result IPF files tend to be
	close in size to more primitive formats).
*/
class IPF: public DiskImage, public TargetPlatform::Distinguisher {
public:
	/*!
		Construct an @c IPF containing content from the file with name @c file_name.

		@throws Storage::FileHolder::Error::CantOpen if this file can't be opened.
		@throws Error::InvalidFormat if the file doesn't appear to contain an .HFE format image.
		@throws Error::UnknownVersion if the file looks correct but is an unsupported version.
	*/
	IPF(const std::string &file_name);

	// implemented to satisfy @c Disk
	HeadPosition maximum_head_position() const;
	int head_count() const;
	std::unique_ptr<Track> track_at_position(Track::Address) const;
	bool represents(const std::string &) const;

private:
	mutable Storage::FileHolder file_;
	uint16_t seek_track(Track::Address address);

	struct TrackDescription {
		long file_offset = 0;
		enum class Density {
			Unknown,
			Noise,
			Auto,
			CopylockAmiga,
			CopylockAmigaNew,
			CopylockST,
			SpeedlockAmiga,
			OldSpeedlockAmiga,
			AdamBrierleyAmiga,
			AdamBrierleyDensityKeyAmiga,

			Max = AdamBrierleyDensityKeyAmiga
		} density = Density::Unknown;
		uint32_t start_bit_pos = 0;
		uint32_t data_bits = 0;
		uint32_t gap_bits = 0;
		uint32_t block_count;
		bool has_fuzzy_bits = false;
	};

	int head_count_;
	int track_count_;
	std::map<Track::Address, TrackDescription> tracks_;
	bool is_sps_format_ = false;

	TargetPlatform::Type target_platforms() final {
		return TargetPlatform::Type(platform_type_);
	}
	TargetPlatform::IntType platform_type_ = TargetPlatform::Amiga;

	Time bit_length(TrackDescription::Density, int block) const;
	void add_gap(std::vector<Storage::Disk::PCMSegment> &, Time bit_length, size_t num_bits, uint32_t value) const;
	void add_unencoded_data(std::vector<Storage::Disk::PCMSegment> &, Time bit_length, size_t num_bits) const;
	void add_raw_data(std::vector<Storage::Disk::PCMSegment> &, Time bit_length, size_t num_bits) const;
};

}
