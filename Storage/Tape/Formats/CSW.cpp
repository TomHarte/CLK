//
//  CSW.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 10/07/2017.
//  Copyright 2017 Thomas Harte. All rights reserved.
//

#include "CSW.hpp"

#include "../../FileHolder.hpp"

#include <cassert>

using namespace Storage::Tape;

CSW::CSW(const std::string &file_name) {
	Storage::FileHolder file(file_name, FileHolder::FileMode::Read);
	if(file.stats().st_size < 0x20) throw ErrorNotCSW;

	// Check signature.
	if(!file.check_signature("Compressed Square Wave")) {
		throw ErrorNotCSW;
	}

	// Check terminating byte.
	if(file.get8() != 0x1a) throw ErrorNotCSW;

	// Get version file number.
	const uint8_t major_version = file.get8();
	const uint8_t minor_version = file.get8();

	// Reject if this is an unknown version.
	if(major_version > 2 || !major_version || minor_version > 1) throw ErrorNotCSW;

	// The header now diverges based on version.
	uint32_t number_of_waves = 0;
	CompressionType compression_type;
	if(major_version == 1) {
		pulse_.length.clock_rate = file.get16le();

		if(file.get8() != 1) throw ErrorNotCSW;
		compression_type = CompressionType::RLE;

		pulse_.type = (file.get8() & 1) ? Pulse::High : Pulse::Low;

		file.seek(0x20, SEEK_SET);
	} else {
		pulse_.length.clock_rate = file.get32le();
		number_of_waves = file.get32le();	// TODO: is this still useful?
		switch(file.get8()) {
			case 1: compression_type = CompressionType::RLE;	break;
			case 2: compression_type = CompressionType::ZRLE;	break;
			default: throw ErrorNotCSW;
		}

		pulse_.type = (file.get8() & 1) ? Pulse::High : Pulse::Low;
		uint8_t extension_length = file.get8();

		if(file.stats().st_size < 0x34 + extension_length) throw ErrorNotCSW;
		file.seek(0x34 + extension_length, SEEK_SET);
	}

	// Grab all data remaining in the file.
	std::vector<uint8_t> file_data;
	const std::size_t remaining_data = size_t(file.stats().st_size) - size_t(file.tell());
	file_data.resize(remaining_data);
	file.read(file_data.data(), remaining_data);

	set_data(std::move(file_data), compression_type);
}

CSW::CSW(std::vector<uint8_t> &&data, CompressionType type, bool initial_level, uint32_t sampling_rate) {
	set_data(std::move(data), type);
	pulse_.length.clock_rate = sampling_rate;
	pulse_.type = initial_level ? Pulse::Type::High : Pulse::Type::Low;
}

void CSW::set_data(std::vector<uint8_t> &&data, CompressionType type) {
	// TODO: compression types.

	if(type == CompressionType::ZRLE) {
		// The only clue given by CSW as to the output size in bytes is that there will be
		// number_of_waves waves. Waves are usually one byte, but may be five. So this code
		// is pessimistic.
//		source_data_.resize(size_t(number_of_waves) * 5);

		// uncompress will tell how many compressed bytes there actually were, so use its
		// modification of output_length to throw away all the memory that isn't actually
		// needed.
//		uLongf output_length = uLongf(number_of_waves * 5);
//		uncompress(source_data_.data(), &output_length, file_data.data(), file_data.size());
//		source_data_.resize(std::size_t(output_length));
	} else {
		source_data_ = std::move(data);
	}

}

std::unique_ptr<FormatSerialiser> CSW::format_serialiser() const {
	return std::make_unique<Serialiser>(source_data_, pulse_);
}

CSW::Serialiser::Serialiser(const std::vector<uint8_t> &data, Pulse pulse) : pulse_(pulse), source_data_(data) {}

uint8_t CSW::Serialiser::get_next_byte() {
	if(source_data_pointer_ == source_data_.size()) return 0xff;

	const uint8_t result = source_data_[source_data_pointer_];
	source_data_pointer_++;
	return result;
}

uint32_t CSW::Serialiser::get_next_int32le() {
	if(source_data_pointer_ > source_data_.size() - 4) return 0xffff;

	const uint32_t result = uint32_t(
		(source_data_[source_data_pointer_ + 0] << 0) |
		(source_data_[source_data_pointer_ + 1] << 8) |
		(source_data_[source_data_pointer_ + 2] << 16) |
		(source_data_[source_data_pointer_ + 3] << 24));
	source_data_pointer_ += 4;
	return result;
}

void CSW::Serialiser::invert_pulse() {
	pulse_.type = (pulse_.type == Pulse::High) ? Pulse::Low : Pulse::High;
}

bool CSW::Serialiser::is_at_end() const {
	return source_data_pointer_ == source_data_.size();
}

void CSW::Serialiser::reset() {
	source_data_pointer_ = 0;
}

Pulse CSW::Serialiser::next_pulse() {
	invert_pulse();
	pulse_.length.length = get_next_byte();
	if(!pulse_.length.length) pulse_.length.length = get_next_int32le();
	return pulse_;
}
