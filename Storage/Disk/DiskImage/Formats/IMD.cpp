//
//  IMD.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 08/12/2023.
//  Copyright © 2023 Thomas Harte. All rights reserved.
//

#include "IMD.hpp"

#include "Storage/Disk/Encodings/MFM/Constants.hpp"
#include "Storage/Disk/Encodings/MFM/Encoder.hpp"
#include "Storage/Disk/Encodings/MFM/SegmentParser.hpp"
#include "Storage/Disk/Track/TrackSerialiser.hpp"

#include <algorithm>

using namespace Storage::Disk;

// Documentation source: https://oldcomputers-ddns.org/public/pub/manuals/imd.pdf

IMD::IMD(const std::string &file_name) : file_(file_name) {
	// Check for signature.
	if(!file_.check_signature<SignatureType::String>("IMD")) {
		throw Error::InvalidFormat;
	}

	// Skip rest of ASCII.
	while(file_.get() != 0x1a);

	// Build track map.
	while(true) {
		const auto location = file_.tell();

		// Skip mode.
		file_.seek(1, Whence::CUR);

		// Grab relevant fields.
		const uint8_t cylinder = file_.get();
		const uint8_t head = file_.get();
		const uint8_t sector_count = file_.get();
		const uint8_t sector_size = file_.get();
		if(file_.eof()) {
			break;
		}

		cylinders_ = std::max(cylinder, cylinders_);
		heads_ = std::max(uint8_t(head & 1), heads_);

		// Update head and cylinder extents, record sector location for later.
		track_locations_.emplace(
			Storage::Disk::Track::Address(head & 1, HeadPosition(cylinder)),
			location);

		// Skip sector numbers.
		file_.seek(sector_count, Whence::CUR);

		// Skip cylinder map.
		if(head & 0x80) {
			file_.seek(sector_count, Whence::CUR);
		}

		// Skip head map.
		if(head & 0x40) {
			file_.seek(sector_count, Whence::CUR);
		}

		// Skip sectors.
		for(int c = 0; c < sector_count; c++) {
			const uint8_t type = file_.get();
			switch(type) {
				case 0x00:	break;	// Sector couldn't be read.

				// Types with all sector data present.
				case 0x01:	case 0x03:	case 0x05:	case 0x07:
					file_.seek(128 << sector_size, Whence::CUR);
				break;

				// Types with a single byte present.
				case 0x02:	case 0x04:	case 0x06:	case 0x08:
					file_.seek(1, Whence::CUR);
				break;
			}
		}
	}

	// Both heads_ and cylinders_ are now the maximum observed IDs, which
	// are one less than the counts.
	++ cylinders_;
	++ heads_;
}

HeadPosition IMD::maximum_head_position() const {
	return HeadPosition(cylinders_);
}

int IMD::head_count() const {
	return heads_ + 1;
}

bool IMD::represents(const std::string &name) const {
	return name == file_.name();
}

std::unique_ptr<Track> IMD::track_at_position(const Track::Address address) const {
	auto location = track_locations_.find(address);
	if(location == track_locations_.end()) {
		return nullptr;
	}

	// Seek to track, parse fully this time.
	file_.seek(location->second, Whence::SET);

	const uint8_t mode = file_.get();
	const uint8_t cylinder = file_.get();
	const uint8_t head = file_.get();
	const uint8_t sector_count = file_.get();
	const uint8_t sector_size = file_.get();

	const std::vector<uint8_t> sector_ids = file_.read(sector_count);
	const std::vector<uint8_t> cylinders = (head & 0x80) ? file_.read(sector_count) : std::vector<uint8_t>{};
	const std::vector<uint8_t> heads = (head & 0x40) ? file_.read(sector_count) : std::vector<uint8_t>{};

	std::vector<Storage::Encodings::MFM::Sector> sectors;
	sectors.reserve(sector_count);

	for(size_t c = 0; c < sector_count; c++) {
		sectors.emplace_back();
		Storage::Encodings::MFM::Sector &sector = sectors.back();

		// Set up sector address.
		sector.address.track = cylinders.empty() ? cylinder : cylinders[c];
		sector.address.side = heads.empty() ? head & 1 : heads[c];
		sector.address.sector = sector_ids[c];
		sector.size = sector_size;

		const auto byte_size = size_t(128 << sector_size);
		uint8_t type = file_.get();

		// Type 0: sector was present, but couldn't be read.
		// Since body CRC errors are a separate item, just don't include a body at all.
		if(!type) {
			continue;
		}

		// Decrement type to turn it into a bit field:
		//
		//	b0 => is compressed within disk image;
		//	b1 => had deleted address mark;
		//	b2 => had data CRC error.
		--type;
		sector.is_deleted = type & 2;
		sector.has_data_crc_error = type & 4;
		if(type & 1) {
			sector.samples.emplace_back(byte_size, file_.get());
		} else {
			sector.samples.push_back(file_.read(byte_size));
		}
	}

	// Mode also indicates data density, but I don't have a good strategy for reconciling that if
	// it were to disagree with the density implied by the quantity of sectors. So a broad 'is it MFM' test is
	// applied only.
	using namespace Storage::Encodings::MFM;
	if(mode < 3) {
		return TrackWithSectors(Density::Single, sectors);
	}
	if(sector_size * sector_count >= 6912) {
		return TrackWithSectors(Density::High, sectors);
	} else {
		return TrackWithSectors(Density::Double, sectors);
	}
}
