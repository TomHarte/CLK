//
//  Video.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 14/04/2018.
//  Copyright 2018 Thomas Harte. All rights reserved.
//

#include "Video.hpp"

using namespace Apple::II::Video;

VideoBase::VideoBase(bool is_iie, std::function<void(Cycles)> &&target) :
	crt_(910, 1, Outputs::Display::Type::NTSC60, Outputs::Display::InputDataType::Luminance1),
	is_iie_(is_iie),
	deferrer_(std::move(target)) {

	// Show only the centre 75% of the TV frame.
	crt_.set_display_type(Outputs::Display::DisplayType::CompositeColour);
	crt_.set_visible_area(Outputs::Display::Rect(0.118f, 0.122f, 0.77f, 0.77f));

	// TODO: there seems to be some sort of bug whereby switching modes can cause
	// a signal discontinuity that knocks phase out of whack. So it isn't safe to
	// use default_colour_bursts elsewhere, though it otherwise should be. If/when
	// it is, start doing so and return to setting the immediate phase up here.
//	crt_.set_immediate_default_phase(0.5f);

	character_zones[0].xor_mask = 0;
	character_zones[0].address_mask = 0x3f;
	character_zones[1].xor_mask = 0;
	character_zones[1].address_mask = 0x3f;
	character_zones[2].xor_mask = 0;
	character_zones[2].address_mask = 0x3f;
	character_zones[3].xor_mask = 0;
	character_zones[3].address_mask = 0x3f;

	if(is_iie) {
		character_zones[0].xor_mask =
		character_zones[2].xor_mask =
		character_zones[3].xor_mask = 0xff;
		character_zones[2].address_mask =
		character_zones[3].address_mask = 0xff;
	}
}

void VideoBase::set_scan_target(Outputs::Display::ScanTarget *scan_target) {
	crt_.set_scan_target(scan_target);
}

Outputs::Display::ScanStatus VideoBase::get_scaled_scan_status() const {
	return crt_.get_scaled_scan_status() / 14.0f;
}

void VideoBase::set_display_type(Outputs::Display::DisplayType display_type) {
	crt_.set_display_type(display_type);
}

/*
	Rote setters and getters.
*/
void VideoBase::set_alternative_character_set(bool alternative_character_set) {
	set_alternative_character_set_ = alternative_character_set;
	deferrer_.defer(Cycles(2), [=] {
		alternative_character_set_ = alternative_character_set;
		if(alternative_character_set) {
			character_zones[1].address_mask = 0xff;
			character_zones[1].xor_mask = 0;
		} else {
			character_zones[1].address_mask = 0x3f;
			character_zones[1].xor_mask = flash_mask();
		}
	});
}

bool VideoBase::get_alternative_character_set() {
	return set_alternative_character_set_;
}

void VideoBase::set_80_columns(bool columns_80) {
	set_columns_80_ = columns_80;
	deferrer_.defer(Cycles(2), [=] {
		columns_80_ = columns_80;
	});
}

bool VideoBase::get_80_columns() {
	return set_columns_80_;
}

void VideoBase::set_80_store(bool store_80) {
	set_store_80_ = store_80_ = store_80;
}

bool VideoBase::get_80_store() {
	return set_store_80_;
}

void VideoBase::set_page2(bool page2) {
	set_page2_ = page2_ = page2;
}

bool VideoBase::get_page2() {
	return set_page2_;
}

void VideoBase::set_text(bool text) {
	set_text_ = text;
	deferrer_.defer(Cycles(2), [=] {
		text_ = text;
	});
}

bool VideoBase::get_text() {
	return set_text_;
}

void VideoBase::set_mixed(bool mixed) {
	set_mixed_ = mixed;
	deferrer_.defer(Cycles(2), [=] {
		mixed_ = mixed;
	});
}

bool VideoBase::get_mixed() {
	return set_mixed_;
}

void VideoBase::set_high_resolution(bool high_resolution) {
	set_high_resolution_ = high_resolution;
	deferrer_.defer(Cycles(2), [=] {
		high_resolution_ = high_resolution;
	});
}

bool VideoBase::get_high_resolution() {
	return set_high_resolution_;
}

void VideoBase::set_annunciator_3(bool annunciator_3) {
	set_annunciator_3_ = annunciator_3;
	deferrer_.defer(Cycles(2), [=] {
		annunciator_3_ = annunciator_3;
		high_resolution_mask_ = annunciator_3_ ? 0x7f : 0xff;
	});
}

bool VideoBase::get_annunciator_3() {
	return set_annunciator_3_;
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

void VideoBase::output_text(uint8_t *target, const uint8_t *const source, size_t length, size_t pixel_row) const {
	for(size_t c = 0; c < length; ++c) {
		const int character = source[c] & character_zones[source[c] >> 6].address_mask;
		const uint8_t xor_mask = character_zones[source[c] >> 6].xor_mask;
		const std::size_t character_address = static_cast<std::size_t>(character << 3) + pixel_row;
		const uint8_t character_pattern = character_rom_[character_address] ^ xor_mask;

		// The character ROM is output MSB to LSB rather than LSB to MSB.
		target[0] = target[1] = character_pattern & 0x40;
		target[2] = target[3] = character_pattern & 0x20;
		target[4] = target[5] = character_pattern & 0x10;
		target[6] = target[7] = character_pattern & 0x08;
		target[8] = target[9] = character_pattern & 0x04;
		target[10] = target[11] = character_pattern & 0x02;
		target[12] = target[13] = character_pattern & 0x01;
		graphics_carry_ = character_pattern & 0x01;
		target += 14;
	}
}

void VideoBase::output_double_text(uint8_t *target, const uint8_t *const source, const uint8_t *const auxiliary_source, size_t length, size_t pixel_row) const {
	for(size_t c = 0; c < length; ++c) {
		const std::size_t character_addresses[2] = {
			static_cast<std::size_t>(
				(auxiliary_source[c] & character_zones[auxiliary_source[c] >> 6].address_mask) << 3
			) + pixel_row,
			static_cast<std::size_t>(
				(source[c] & character_zones[source[c] >> 6].address_mask) << 3
			) + pixel_row
		};

		const uint8_t character_patterns[2] = {
			static_cast<uint8_t>(
				character_rom_[character_addresses[0]] ^ character_zones[auxiliary_source[c] >> 6].xor_mask
			),
			static_cast<uint8_t>(
				character_rom_[character_addresses[1]] ^ character_zones[source[c] >> 6].xor_mask
			)
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
}

void VideoBase::output_low_resolution(uint8_t *target, const uint8_t *const source, size_t length, int column, int row) const {
	const int row_shift = row&4;
	for(size_t c = 0; c < length; ++c) {
		// Low-resolution graphics mode shifts the colour code on a loop, but has to account for whether this
		// 14-sample output window is starting at the beginning of a colour cycle or halfway through.
		if((column + static_cast<int>(c))&1) {
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
}

void VideoBase::output_fat_low_resolution(uint8_t *target, const uint8_t *const source, size_t length, int column, int row) const {
	const int row_shift = row&4;
	for(size_t c = 0; c < length; ++c) {
		// Fat low-resolution mode appears not to do anything to try to make odd and
		// even columns compatible.
		target[0] = target[1] = target[8] = target[9] = (source[c] >> row_shift) & 1;
		target[2] = target[3] = target[10] = target[11] = (source[c] >> row_shift) & 2;
		target[4] = target[5] = target[12] = target[13] = (source[c] >> row_shift) & 4;
		target[6] = target[7] = (source[c] >> row_shift) & 8;
		graphics_carry_ = (source[c] >> row_shift) & 4;
		target += 14;
	}
}

void VideoBase::output_double_low_resolution(uint8_t *target, const uint8_t *const source, const uint8_t *const auxiliary_source, size_t length, int column, int row) const {
	const int row_shift = row&4;
	for(size_t c = 0; c < length; ++c) {
		if((column + static_cast<int>(c))&1) {
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
}

void VideoBase::output_high_resolution(uint8_t *target, const uint8_t *const source, size_t length) const {
	for(size_t c = 0; c < length; ++c) {
		// High resolution graphics shift out LSB to MSB, optionally with a delay of half a pixel.
		// If there is a delay, the previous output level is held to bridge the gap.
		// Delays may be ignored on a IIe if Annunciator 3 is set; that's the state that
		// high_resolution_mask_ models.
		if(source[c] & high_resolution_mask_ & 0x80) {
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
}

void VideoBase::output_double_high_resolution(uint8_t *target, const uint8_t *const source, const uint8_t *const auxiliary_source, size_t length) const {
	for(size_t c = 0; c < length; ++c) {
		target[0] = auxiliary_source[c] & 0x01;
		target[1] = auxiliary_source[c] & 0x02;
		target[2] = auxiliary_source[c] & 0x04;
		target[3] = auxiliary_source[c] & 0x08;
		target[4] = auxiliary_source[c] & 0x10;
		target[5] = auxiliary_source[c] & 0x20;
		target[6] = auxiliary_source[c] & 0x40;
		target[7] = source[c] & 0x01;
		target[8] = source[c] & 0x02;
		target[9] = source[c] & 0x04;
		target[10] = source[c] & 0x08;
		target[11] = source[c] & 0x10;
		target[12] = source[c] & 0x20;
		target[13] = source[c] & 0x40;

		graphics_carry_ = auxiliary_source[c] & 0x40;
		target += 14;
	}
}
