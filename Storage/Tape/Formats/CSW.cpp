//
//  CSW.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 10/07/2017.
//  Copyright © 2017 Thomas Harte. All rights reserved.
//

#include "CSW.hpp"

using namespace Storage::Tape;

CSW::CSW(const char *file_name) :
	Storage::FileHolder(file_name) {
	if(file_stats_.st_size < 0x20) throw ErrorNotCSW;

	// Check signature.
	char identifier[22];
	char signature[] = "Compressed Square Wave";
	fread(identifier, 1, 22, file_);
	if(memcmp(identifier, signature, strlen(signature))) throw ErrorNotCSW;

	// Check terminating byte.
	if(fgetc(file_) != 0x1a) throw ErrorNotCSW;

	// Get version file number.
	uint8_t major_version = (uint8_t)fgetc(file_);
	uint8_t minor_version = (uint8_t)fgetc(file_);

	// Reject if this is an unknown version.
	if(major_version > 2 || !major_version || minor_version > 1) throw ErrorNotCSW;

	// The header now diverges based on version.
	if(major_version == 1) {
		pulse_.length.clock_rate = fgetc16le();

		if(fgetc(file_) != 1) throw ErrorNotCSW;
		compression_type_ = RLE;

		pulse_.type = (fgetc(file_) & 1) ? Pulse::High : Pulse::Low;

		fseek(file_, 0x20, SEEK_SET);
	} else {
		pulse_.length.clock_rate = fgetc32le();
		number_of_waves_ = fgetc32le();
		switch(fgetc(file_)) {
			case 1: compression_type_ = RLE;	break;
			case 2: compression_type_ = ZRLE;	break;
			default: throw ErrorNotCSW;
		}

		pulse_.type = (fgetc(file_) & 1) ? Pulse::High : Pulse::Low;
		uint8_t extension_length = (uint8_t)fgetc(file_);

		if(file_stats_.st_size < 0x34 + extension_length) throw ErrorNotCSW;
		fseek(file_, 0x34 + extension_length, SEEK_SET);
	}

	if(compression_type_ == ZRLE) {
		inflation_stream_.zalloc = Z_NULL;
		inflation_stream_.zfree = Z_NULL;
		inflation_stream_.opaque = Z_NULL;
		inflation_stream_.avail_in = 0;
		inflation_stream_.next_in = Z_NULL;
		int result = inflateInit(&inflation_stream_);
		if(result != Z_OK) throw ErrorNotCSW;
	}
}

CSW::~CSW() {
	if(compression_type_ == ZRLE) {
		inflateEnd(&inflation_stream_);
	}
}


bool CSW::is_at_end() {
	return true;
}

void CSW::virtual_reset() {
}

Tape::Pulse CSW::virtual_get_next_pulse() {
	Tape::Pulse pulse;
	return pulse;
}
