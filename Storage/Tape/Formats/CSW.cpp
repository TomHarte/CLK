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
	Storage::FileHolder(file_name),
	source_data_pointer_(0) {
	if(file_stats_.st_size < 0x20) throw ErrorNotCSW;

	// Check signature.
	char identifier[22];
	char signature[] = "Compressed Square Wave";
	fread(identifier, 1, strlen(signature), file_);
	if(memcmp(identifier, signature, strlen(signature))) throw ErrorNotCSW;

	// Check terminating byte.
	if(fgetc(file_) != 0x1a) throw ErrorNotCSW;

	// Get version file number.
	uint8_t major_version = static_cast<uint8_t>(fgetc(file_));
	uint8_t minor_version = static_cast<uint8_t>(fgetc(file_));

	// Reject if this is an unknown version.
	if(major_version > 2 || !major_version || minor_version > 1) throw ErrorNotCSW;

	// The header now diverges based on version.
	uint32_t number_of_waves = 0;
	if(major_version == 1) {
		pulse_.length.clock_rate = fgetc16le();

		if(fgetc(file_) != 1) throw ErrorNotCSW;
		compression_type_ = RLE;

		pulse_.type = (fgetc(file_) & 1) ? Pulse::High : Pulse::Low;

		fseek(file_, 0x20, SEEK_SET);
	} else {
		pulse_.length.clock_rate = fgetc32le();
		number_of_waves = fgetc32le();
		switch(fgetc(file_)) {
			case 1: compression_type_ = RLE;	break;
			case 2: compression_type_ = ZRLE;	break;
			default: throw ErrorNotCSW;
		}

		pulse_.type = (fgetc(file_) & 1) ? Pulse::High : Pulse::Low;
		uint8_t extension_length = static_cast<uint8_t>(fgetc(file_));

		if(file_stats_.st_size < 0x34 + extension_length) throw ErrorNotCSW;
		fseek(file_, 0x34 + extension_length, SEEK_SET);
	}

	if(compression_type_ == ZRLE) {
		// The only clue given by CSW as to the output size in bytes is that there will be
		// number_of_waves waves. Waves are usually one byte, but may be five. So this code
		// is pessimistic.
		source_data_.resize(static_cast<size_t>(number_of_waves) * 5);

		std::vector<uint8_t> file_data;
		size_t remaining_data = static_cast<size_t>(file_stats_.st_size) - static_cast<size_t>(ftell(file_));
		file_data.resize(remaining_data);
		fread(file_data.data(), sizeof(uint8_t), remaining_data, file_);

		// uncompress will tell how many compressed bytes there actually were, so use its
		// modification of output_length to throw away all the memory that isn't actually
		// needed.
		uLongf output_length = static_cast<uLongf>(number_of_waves * 5);
		uncompress(source_data_.data(), &output_length, file_data.data(), file_data.size());
		source_data_.resize(static_cast<size_t>(output_length));
	} else {
		rle_start_ = ftell(file_);
	}

	invert_pulse();
}

uint8_t CSW::get_next_byte() {
	switch(compression_type_) {
		case RLE: return static_cast<uint8_t>(fgetc(file_));
		case ZRLE: {
			if(source_data_pointer_ == source_data_.size()) return 0xff;
			uint8_t result = source_data_[source_data_pointer_];
			source_data_pointer_++;
			return result;
		}
	}
}

uint32_t CSW::get_next_int32le() {
	switch(compression_type_) {
		case RLE: return fgetc32le();
		case ZRLE: {
			if(source_data_pointer_ > source_data_.size() - 4) return 0xffff;
			uint32_t result = (uint32_t)(
				(source_data_[source_data_pointer_ + 0] << 0) |
				(source_data_[source_data_pointer_ + 1] << 8) |
				(source_data_[source_data_pointer_ + 2] << 16) |
				(source_data_[source_data_pointer_ + 3] << 24));
			source_data_pointer_ += 4;
			return result;
		}
	}
}

void CSW::invert_pulse() {
	pulse_.type = (pulse_.type == Pulse::High) ? Pulse::Low : Pulse::High;
}

bool CSW::is_at_end() {
	switch(compression_type_) {
		case RLE: return (bool)feof(file_);
		case ZRLE: return source_data_pointer_ == source_data_.size();
	}
}

void CSW::virtual_reset() {
	switch(compression_type_) {
		case RLE:	fseek(file_, rle_start_, SEEK_SET);	break;
		case ZRLE:	source_data_pointer_ = 0;			break;
	}
}

Tape::Pulse CSW::virtual_get_next_pulse() {
	invert_pulse();
	pulse_.length.length = get_next_byte();
	if(!pulse_.length.length) pulse_.length.length = get_next_int32le();
	return pulse_;
}
