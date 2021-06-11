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
	VideoSwitches<Cycles>(is_iie, Cycles(2), std::move(target)),
	crt_(910, 1, Outputs::Display::Type::NTSC60, Outputs::Display::InputDataType::Luminance1),
	is_iie_(is_iie) {

	crt_.set_display_type(Outputs::Display::DisplayType::CompositeColour);
	set_use_square_pixels(use_square_pixels_);

	// TODO: there seems to be some sort of bug whereby switching modes can cause
	// a signal discontinuity that knocks phase out of whack. So it isn't safe to
	// use default_colour_bursts elsewhere, though it otherwise should be. If/when
	// it is, start doing so and return to setting the immediate phase up here.
//	crt_.set_immediate_default_phase(0.5f);
}

void VideoBase::set_use_square_pixels(bool use_square_pixels) {
	use_square_pixels_ = use_square_pixels;

	// HYPER-UGLY HACK. See correlated hack in the Macintosh.
#ifdef __APPLE__
	crt_.set_visible_area(Outputs::Display::Rect(0.128f, 0.122f, 0.75f, 0.77f));
#else
	if(use_square_pixels) {
		crt_.set_visible_area(Outputs::Display::Rect(0.128f, 0.112f, 0.75f, 0.73f));
	} else {
		crt_.set_visible_area(Outputs::Display::Rect(0.128f, 0.12f, 0.75f, 0.77f));
	}
#endif

	if(use_square_pixels) {
		// From what I can make out, many contemporary Apple II monitors were
		// calibrated slightly to stretch the Apple II's display slightly wider
		// than it should be per the NTSC standards, for approximately square
		// pixels. This reproduces that.

		// 243 lines and 52µs are visible.
		// i.e. to be square, 1 pixel should be: (1/243 * 52) * (3/4) = 156/972 = 39/243 µs
		// On an Apple II each pixel is actually 1/7µs.
		// Therefore the adjusted aspect ratio should be (4/3) * (39/243)/(1/7) = (4/3) * 273/243 = 1092/729 = 343/243 ~= 1.412
		crt_.set_aspect_ratio(343.0f / 243.0f);
	} else {
		// Standard NTSC aspect ratio.
		crt_.set_aspect_ratio(4.0f / 3.0f);
	}
}
bool VideoBase::get_use_square_pixels() {
	return use_square_pixels_;
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

Outputs::Display::DisplayType VideoBase::get_display_type() const {
	return crt_.get_display_type();
}

void VideoBase::output_text(uint8_t *target, const uint8_t *const source, size_t length, size_t pixel_row) const {
	for(size_t c = 0; c < length; ++c) {
		const int character = source[c] & character_zones_[source[c] >> 6].address_mask;
		const uint8_t xor_mask = character_zones_[source[c] >> 6].xor_mask;
		const std::size_t character_address = size_t(character << 3) + pixel_row;
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
			size_t(
				(auxiliary_source[c] & character_zones_[auxiliary_source[c] >> 6].address_mask) << 3
			) + pixel_row,
			size_t(
				(source[c] & character_zones_[source[c] >> 6].address_mask) << 3
			) + pixel_row
		};

		const uint8_t character_patterns[2] = {
			uint8_t(
				character_rom_[character_addresses[0]] ^ character_zones_[auxiliary_source[c] >> 6].xor_mask
			),
			uint8_t(
				character_rom_[character_addresses[1]] ^ character_zones_[source[c] >> 6].xor_mask
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
		if((column + int(c))&1) {
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

void VideoBase::output_fat_low_resolution(uint8_t *target, const uint8_t *const source, size_t length, int, int row) const {
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
		if((column + int(c))&1) {
			target[0] = target[4] = (auxiliary_source[c] >> row_shift) & 4;
			target[1] = target[5] = (auxiliary_source[c] >> row_shift) & 8;
			target[2] = target[6] = (auxiliary_source[c] >> row_shift) & 1;
			target[3] = (auxiliary_source[c] >> row_shift) & 2;

			target[8] = target[12] = (source[c] >> row_shift) & 8;
			target[9] = target[13] = (source[c] >> row_shift) & 1;
			target[10] = (source[c] >> row_shift) & 2;
			target[7] = target[11] = (source[c] >> row_shift) & 4;
			graphics_carry_ = (source[c] >> row_shift) & 8;
		} else {
			target[0] = target[4] = (auxiliary_source[c] >> row_shift) & 1;
			target[1] = target[5] = (auxiliary_source[c] >> row_shift) & 2;
			target[2] = target[6] = (auxiliary_source[c] >> row_shift) & 4;
			target[3] = (auxiliary_source[c] >> row_shift) & 8;

			target[8] = target[12] = (source[c] >> row_shift) & 2;
			target[9] = target[13] = (source[c] >> row_shift) & 4;
			target[10] = (source[c] >> row_shift) & 8;
			target[7] = target[11] = (source[c] >> row_shift) & 1;
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
