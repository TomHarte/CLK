//
//  DMK.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 08/01/2018.
//  Copyright © 2018 Thomas Harte. All rights reserved.
//

#include "DMK.hpp"

using namespace Storage::Disk;

DMK::DMK(const char *file_name) :
	file_(file_name) {
	// Determine whether this DMK represents a read-only disk (whether intentionally,
	// or by virtue of placement).
	uint8_t read_only_byte = file_.get8();
	if(read_only_byte != 0x00 && read_only_byte != 0xff) throw ErrorNotDMK;
	is_read_only_ = (read_only_byte == 0xff) || file_.get_is_known_read_only();

	// Read track count and size.
	head_position_count_ = static_cast<int>(file_.get8());
	track_length_ = static_cast<long>(file_.get16le());

	// Track length must be at least 0x80, as that's the size of the IDAM
	// table before track contents.
	if(track_length_ < 0x80) throw ErrorNotDMK;

	// Read the file flags and apply them.
	uint8_t flags = file_.get8();
	head_count_ = 2 - ((flags & 0x10) >> 4);
	head_position_count_ /= head_count_;
	is_purely_single_density_ = !!(flags & 0x40);

	// Skip to the end of the header and check that this is
	// "in the emulator's native format".
	file_.seek(0xc, SEEK_SET);
	uint32_t format = file_.get32le();
	if(format) throw ErrorNotDMK;
}

int DMK::get_head_position_count() {
	return head_position_count_;
}

int DMK::get_head_count() {
	return head_count_;
}

bool DMK::get_is_read_only() {
	return is_read_only_;
}

long DMK::get_file_offset_for_position(Track::Address address) {
	return (address.head*head_count_ + address.position) * track_length_ + 16;
}

std::shared_ptr<::Storage::Disk::Track> DMK::get_track_at_position(::Storage::Disk::Track::Address address) {
	file_.seek(get_file_offset_for_position(address), SEEK_SET);

	uint16_t idam_locations[64];
	std::size_t index = 0;
	for(std::size_t c = 0; c < sizeof(idam_locations); ++c) {
		idam_locations[index] = file_.get16le();
		if((idam_locations[index] & 0x7fff) >= 128) {
			index++;
		}
	}

	// `index` is now the final (sensical) entry in the IDAM location table.
	// TODO: parse track contents.
	printf("number of IDAMs: %d", index);

	return nullptr;
}
