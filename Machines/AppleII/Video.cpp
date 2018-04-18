//
//  Video.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 14/04/2018.
//  Copyright © 2018 Thomas Harte. All rights reserved.
//

#include "Video.hpp"

using namespace AppleII::Video;

namespace {

struct ScaledByteFiller {
	ScaledByteFiller() {
		VideoBase::setup_tables();
	}
} throwaway;

}

VideoBase::VideoBase() :
	crt_(new Outputs::CRT::CRT(455, 1, Outputs::CRT::DisplayType::NTSC60, 1)) {

	// Set a composite sampling function that assumes 1bpp input, and uses just 7 bits per byte.
	crt_->set_composite_sampling_function(
		"float composite_sample(usampler2D sampler, vec2 coordinate, vec2 icoordinate, float phase, float amplitude)"
		"{"
			"uint texValue = texture(sampler, coordinate).r;"
			"texValue <<= uint(icoordinate.x * 7.0) % 7u;"
			"return float(texValue & 64u);"
		"}");

		// TODO: the above has precision issues. Fix!

	// Show only the centre 75% of the TV frame.
	crt_->set_video_signal(Outputs::CRT::VideoSignal::Composite);
	crt_->set_visible_area(Outputs::CRT::Rect(0.115f, 0.117f, 0.77f, 0.77f));
}

Outputs::CRT::CRT *VideoBase::get_crt() {
	return crt_.get();
}

uint16_t VideoBase::scaled_byte[256];
uint16_t VideoBase::low_resolution_patterns[2][16];

void VideoBase::setup_tables() {
	for(int c = 0; c < 128; ++c) {
		const uint16_t value =
			((c & 0x01) ? 0x0003 : 0x0000) |
			((c & 0x02) ? 0x000c : 0x0000) |
			((c & 0x04) ? 0x0030 : 0x0000) |
			((c & 0x08) ? 0x0140 : 0x0000) |
			((c & 0x10) ? 0x0600 : 0x0000) |
			((c & 0x20) ? 0x1800 : 0x0000) |
			((c & 0x40) ? 0x6000 : 0x0000);

		uint8_t *const table_entry = reinterpret_cast<uint8_t *>(&scaled_byte[c]);
		table_entry[1] = static_cast<uint8_t>(value & 0xff);
		table_entry[0] = static_cast<uint8_t>(value >> 8);
	}
	for(int c = 128; c < 256; ++c) {
		uint8_t *const source_table_entry = reinterpret_cast<uint8_t *>(&scaled_byte[c & 0x7f]);
		uint8_t *const destination_table_entry = reinterpret_cast<uint8_t *>(&scaled_byte[c]);

		destination_table_entry[0] = static_cast<uint8_t>(source_table_entry[0] >> 1);
		destination_table_entry[1] = static_cast<uint8_t>((source_table_entry[1] >> 1) | ((source_table_entry[0]&1) << 6));
	}

	for(int c = 0; c < 16; ++c) {
		uint8_t *table_entry = reinterpret_cast<uint8_t *>(&low_resolution_patterns[0][c]);
		table_entry[1] = static_cast<uint8_t>(c | (c << 4));
		table_entry[0] = static_cast<uint8_t>((c >> 3) | (c << 1) | (c << 5));

		table_entry = reinterpret_cast<uint8_t *>(&low_resolution_patterns[1][c]);
		table_entry[1] = static_cast<uint8_t>((c >> 2) | (c << 2) | (c << 6));
		table_entry[0] = static_cast<uint8_t>((c >> 1) | (c << 3));
	}
}

void VideoBase::set_graphics_mode() {
	use_graphics_mode_ = true;
}

void VideoBase::set_text_mode() {
	use_graphics_mode_ = false;
}

void VideoBase::set_mixed_mode(bool mixed_mode) {
	mixed_mode_ = mixed_mode;
}

void VideoBase::set_video_page(int page) {
	video_page_ = page;
}

void VideoBase::set_low_resolution() {
	graphics_mode_ = GraphicsMode::LowRes;
}

void VideoBase::set_high_resolution() {
	graphics_mode_ = GraphicsMode::HighRes;
}

void VideoBase::set_character_rom(const std::vector<uint8_t> &character_rom) {
	character_rom_ = character_rom;
}
