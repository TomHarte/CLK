//
//  Video.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 14/04/2018.
//  Copyright 2018 Thomas Harte. All rights reserved.
//

#include "Video.hpp"

using namespace AppleII::Video;

VideoBase::VideoBase(bool is_iie) :
	crt_(new Outputs::CRT::CRT(910, 1, Outputs::CRT::DisplayType::NTSC60, 1)),
	is_iie_(is_iie) {

	// Set a composite sampling function that assumes one byte per pixel input, and
	// accepts any non-zero value as being fully on, zero being fully off.
	crt_->set_composite_sampling_function(
		"float composite_sample(usampler2D sampler, vec2 coordinate, vec2 icoordinate, float phase, float amplitude)"
		"{"
			"return clamp(texture(sampler, coordinate).r, 0.0, 0.7);"
		"}");

	// Show only the centre 75% of the TV frame.
	crt_->set_video_signal(Outputs::CRT::VideoSignal::Composite);
	crt_->set_visible_area(Outputs::CRT::Rect(0.115f, 0.122f, 0.77f, 0.77f));
	crt_->set_immediate_default_phase(0.0f);
}

Outputs::CRT::CRT *VideoBase::get_crt() {
	return crt_.get();
}

/*
	Rote setters and getters.
*/
void VideoBase::set_alternative_character_set(bool alternative_character_set) {
	alternative_character_set_ = alternative_character_set;
}

bool VideoBase::get_alternative_character_set() {
	return alternative_character_set_;
}

void VideoBase::set_80_columns(bool columns_80) {
	columns_80_ = columns_80;
}

bool VideoBase::get_80_columns() {
	return columns_80_;
}

void VideoBase::set_80_store(bool store_80) {
	store_80_ = store_80;
}

bool VideoBase::get_80_store() {
	return store_80_;
}

void VideoBase::set_page2(bool page2) {
	page2_ = page2;
}

bool VideoBase::get_page2() {
	return page2_;
}

void VideoBase::set_text(bool text) {
	text_ = text;
}

bool VideoBase::get_text() {
	return text_;
}

void VideoBase::set_mixed(bool mixed) {
	mixed_ = mixed;
}

bool VideoBase::get_mixed() {
	return mixed_;
}

void VideoBase::set_high_resolution(bool high_resolution) {
	high_resolution_ = high_resolution;
}

bool VideoBase::get_high_resolution() {
	return high_resolution_;
}

void VideoBase::set_double_high_resolution(bool double_high_resolution) {
	double_high_resolution_ = double_high_resolution;
}

bool VideoBase::get_double_high_resolution() {
	return double_high_resolution_;
}

void VideoBase::set_character_rom(const std::vector<uint8_t> &character_rom) {
	character_rom_ = character_rom;

	// Flip all character contents based on the second line of the $ graphic.
	if(character_rom_[0x121] == 0x3c || character_rom_[0x122] == 0x3c) {
		for(auto &graphic : character_rom_) {
			graphic =
				((graphic & 0x01) ? 0x40 : 0x00) |
				((graphic & 0x02) ? 0x20 : 0x00) |
				((graphic & 0x04) ? 0x10 : 0x00) |
				((graphic & 0x08) ? 0x08 : 0x00) |
				((graphic & 0x10) ? 0x04 : 0x00) |
				((graphic & 0x20) ? 0x02 : 0x00) |
				((graphic & 0x40) ? 0x01 : 0x00);
		}
	}
}

uint8_t *VideoBase::output_text(uint8_t *target, uint8_t *source, size_t length, size_t pixel_row) {
	const uint8_t inverses[] = {
		0xff,
		is_iie_ ? static_cast<uint8_t>(0xff) : static_cast<uint8_t>((flash_ / flash_length) * 0xff),
		is_iie_ ? static_cast<uint8_t>(0xff) : static_cast<uint8_t>(0x00),
		is_iie_ ? static_cast<uint8_t>(0xff) : static_cast<uint8_t>(0x00)
	};
	const int or_mask = alternative_character_set_ ? 0x100 : 0x000;
	const int and_mask = is_iie_ ? ~0 : 0x3f;

	for(size_t c = 0; c < length; ++c) {
		const int character = (source[c] | or_mask) & and_mask;
		const uint8_t xor_mask = inverses[character >> 6];
		const std::size_t character_address = static_cast<std::size_t>(character << 3) + pixel_row;
		const uint8_t character_pattern = character_rom_[character_address] ^ xor_mask;

		// The character ROM is output MSB to LSB rather than LSB to MSB.
		target[0] = character_pattern & 0x40;
		target[1] = character_pattern & 0x20;
		target[2] = character_pattern & 0x10;
		target[3] = character_pattern & 0x08;
		target[4] = character_pattern & 0x04;
		target[5] = character_pattern & 0x02;
		target[6] = character_pattern & 0x01;
		graphics_carry_ = character_pattern & 0x01;
		target += 7;
	}

	return target;
}

uint8_t *VideoBase::output_double_text(uint8_t *target, uint8_t *source, uint8_t *auxiliary_source, size_t length, size_t pixel_row) {
	for(size_t c = 0; c < length; ++c) {
		const std::size_t character_addresses[2] = {
			static_cast<std::size_t>(
				auxiliary_source[c] << 3
			) + pixel_row,
			static_cast<std::size_t>(
				source[c] << 3
			) + pixel_row,
		};

		const size_t pattern_offset = alternative_character_set_ ? (256*8) : 0;
		const uint8_t character_patterns[2] = {
			character_rom_[character_addresses[0] + pattern_offset],
			character_rom_[character_addresses[1] + pattern_offset],
		};

		// The character ROM is output MSB to LSB rather than LSB to MSB.
		target[0] = character_patterns[0] & 0x40;
		target[1] = character_patterns[0] & 0x20;
		target[2] = character_patterns[0] & 0x10;
		target[3] = character_patterns[0] & 0x08;
		target[4] = character_patterns[0] & 0x04;
		target[5] = character_patterns[0] & 0x02;
		target[6] = character_patterns[0] & 0x01;
		target[7] = character_patterns[1] & 0x40;
		target[8] = character_patterns[1] & 0x20;
		target[9] = character_patterns[1] & 0x10;
		target[10] = character_patterns[1] & 0x08;
		target[11] = character_patterns[1] & 0x04;
		target[12] = character_patterns[1] & 0x02;
		target[13] = character_patterns[1] & 0x01;
		graphics_carry_ = character_patterns[1] & 0x01;
		target += 14;
	}

	return target;
}

uint8_t *VideoBase::output_low_resolution(uint8_t *target, uint8_t *source, size_t length, int row) {
	const int row_shift = row&4;
	for(size_t c = 0; c < length; ++c) {
		// Low-resolution graphics mode shifts the colour code on a loop, but has to account for whether this
		// 14-sample output window is starting at the beginning of a colour cycle or halfway through.
		if(c&1) {
			target[0] = target[4] = target[8] = target[12] = (source[c] >> row_shift) & 4;
			target[1] = target[5] = target[9] = target[13] = (source[c] >> row_shift) & 8;
			target[2] = target[6] = target[10] = (source[c] >> row_shift) & 1;
			target[3] = target[7] = target[11] = (source[c] >> row_shift) & 2;
			graphics_carry_ = (source[c] >> row_shift) & 8;
		} else {
			target[0] = target[4] = target[8] = target[12] = (source[c] >> row_shift) & 1;
			target[1] = target[5] = target[9] = target[13] = (source[c] >> row_shift) & 2;
			target[2] = target[6] = target[10] = (source[c] >> row_shift) & 4;
			target[3] = target[7] = target[11] = (source[c] >> row_shift) & 8;
			graphics_carry_ = (source[c] >> row_shift) & 2;
		}
		target += 14;
	}

	return target;
}

uint8_t *VideoBase::output_double_low_resolution(uint8_t *target, uint8_t *source, uint8_t *auxiliary_source, size_t length, int row) {
	const int row_shift = row&4;
	for(size_t c = 0; c < length; ++c) {
		if(c&1) {
			target[0] = target[4] = (auxiliary_source[c] >> row_shift) & 2;
			target[1] = target[5] = (auxiliary_source[c] >> row_shift) & 4;
			target[2] = target[6] = (auxiliary_source[c] >> row_shift) & 8;
			target[3] = (auxiliary_source[c] >> row_shift) & 1;

			target[8] = target[12] = (source[c] >> row_shift) & 4;
			target[9] = target[13] = (source[c] >> row_shift) & 8;
			target[10] = (source[c] >> row_shift) & 1;
			target[7] = target[11] = (source[c] >> row_shift) & 2;
			graphics_carry_ = (source[c] >> row_shift) & 8;
		} else {
			target[0] = target[4] = (auxiliary_source[c] >> row_shift) & 8;
			target[1] = target[5] = (auxiliary_source[c] >> row_shift) & 1;
			target[2] = target[6] = (auxiliary_source[c] >> row_shift) & 2;
			target[3] = (auxiliary_source[c] >> row_shift) & 4;

			target[8] = target[12] = (source[c] >> row_shift) & 1;
			target[9] = target[13] = (source[c] >> row_shift) & 2;
			target[10] = (source[c] >> row_shift) & 4;
			target[7] = target[11] = (source[c] >> row_shift) & 8;
			graphics_carry_ = (source[c] >> row_shift) & 2;
		}
		target += 14;
	}

	return target;
}

uint8_t *VideoBase::output_high_resolution(uint8_t *target, uint8_t *source, size_t length) {
	for(size_t c = 0; c < length; ++c) {
		// High resolution graphics shift out LSB to MSB, optionally with a delay of half a pixel.
		// If there is a delay, the previous output level is held to bridge the gap.
		if(base_stream_[c] & 0x80) {
			target[0] = graphics_carry_;
			target[1] = target[2] = source[c] & 0x01;
			target[3] = target[4] = source[c] & 0x02;
			target[5] = target[6] = source[c] & 0x04;
			target[7] = target[8] = source[c] & 0x08;
			target[9] = target[10] = source[c] & 0x10;
			target[11] = target[12] = source[c] & 0x20;
			target[13] = source[c] & 0x40;
		} else {
			target[0] = target[1] = source[c] & 0x01;
			target[2] = target[3] = source[c] & 0x02;
			target[4] = target[5] = source[c] & 0x04;
			target[6] = target[7] = source[c] & 0x08;
			target[8] = target[9] = source[c] & 0x10;
			target[10] = target[11] = source[c] & 0x20;
			target[12] = target[13] = source[c] & 0x40;
		}
		graphics_carry_ = source[c] & 0x40;
		target += 14;
	}
	return target;
}

uint8_t *VideoBase::output_double_high_resolution(uint8_t *target, uint8_t *source, uint8_t *auxiliary_source, size_t length) {
	for(size_t c = 0; c < length; ++c) {
		target[0] = graphics_carry_;
		target[1] = auxiliary_source[c] & 0x01;
		target[2] = auxiliary_source[c] & 0x02;
		target[3] = auxiliary_source[c] & 0x04;
		target[4] = auxiliary_source[c] & 0x08;
		target[5] = auxiliary_source[c] & 0x10;
		target[6] = auxiliary_source[c] & 0x20;
		target[7] = auxiliary_source[c] & 0x40;
		target[8] = auxiliary_source[c] & 0x01;
		target[9] = auxiliary_source[c] & 0x02;
		target[10] = auxiliary_source[c] & 0x04;
		target[11] = auxiliary_source[c] & 0x08;
		target[12] = auxiliary_source[c] & 0x10;
		target[13] = auxiliary_source[c] & 0x20;
		graphics_carry_ = auxiliary_source[c] & 0x40;
		pixel_pointer_ += 14;
	}

	return target;
}
