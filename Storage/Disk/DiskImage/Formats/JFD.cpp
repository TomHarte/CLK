//
//  JFD.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 29/05/2025.
//  Copyright © 2025 Thomas Harte. All rights reserved.
//

#include "JFD.hpp"

#include "Storage/Disk/Encodings/MFM/Constants.hpp"
#include "Storage/Disk/Encodings/MFM/Encoder.hpp"
#include "Storage/Disk/Encodings/MFM/Sector.hpp"

using namespace Storage::Disk;

uint32_t JFD::read32() const {
	uint8_t bytes[4];
	gzread(file_, bytes, 4);
	return uint32_t(bytes[0] | (bytes[1] << 8) | (bytes[2] << 16) | (bytes[3] << 24));
}

uint8_t JFD::read8() const {
	return uint8_t(gzgetc(file_));
}

JFD::JFD(const std::string &file_name) : file_name_(file_name) {
	file_ = gzopen(file_name.c_str(), "rb");

	// First four bytes: signature.
	uint8_t signature[4];
	constexpr uint8_t required_signature[4] = { 'J', 'F', 'D', 'I' };
	gzread(file_, signature, sizeof(signature));
	if(!std::equal(std::begin(signature), std::end(signature), std::begin(required_signature))) {
		throw 1;
	}

	gzseek(file_, 24, SEEK_SET);
	track_offset_ = read32();
	sector_offset_ = read32();
	data_offset_ = read32();
	// TODO: grab delta tables, once I know how to parse them.
}

HeadPosition JFD::maximum_head_position() const {
	return HeadPosition{
		int((sector_offset_ - track_offset_) / (2 * sizeof(uint32_t)))
	};
}

int JFD::head_count() const {
	return 2;
}

std::unique_ptr<Track> JFD::track_at_position(const Track::Address address) const {
	const uint32_t offset = track_offset_ + uint32_t(address.head + address.position.as_int() * 2);

	if(offset >= sector_offset_) {
		return nullptr;
	}
	gzseek(file_, offset, SEEK_SET);

	const uint32_t sector_begin = read32();
	if(sector_begin == 0xffffffff) {
		return nullptr;
	}

	std::vector<Storage::Encodings::MFM::Sector> sectors;
	uint32_t sector = sector_begin;
	while(sector < sector_offset_) {
		gzseek(file_, sector_offset_ + sector, SEEK_SET);

		const auto crc_size = read8();
		const auto sector_number = read8();
		const auto options_density = read8();
		const auto time_ms = read8();
		if(crc_size == 0xff && sector_number == 0xff && options_density == 0xff && time_ms == 0xff) {
			break;
		}

		const auto data = read32();
		printf("After %d ms, od %02x, sector %d, crc_size %02x, at %d\n", time_ms, options_density, sector_number, crc_size, data);

		sector += 8;
	}

	return {};
}

bool JFD::represents(const std::string &name) const {
	return name == file_name_;
}
