//
//  Video.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 14/04/2018.
//  Copyright 2018 Thomas Harte. All rights reserved.
//

#include "Video.hpp"

using namespace AppleII::Video;

VideoBase::VideoBase() :
	crt_(new Outputs::CRT::CRT(910, 1, Outputs::CRT::DisplayType::NTSC60, 1)) {

	// Set a composite sampling function that assumes one byte per pixel input, and
	// accepts any non-zero value as being fully on, zero being fully off.
	crt_->set_composite_sampling_function(
		"float composite_sample(usampler2D sampler, vec2 coordinate, vec2 icoordinate, float phase, float amplitude)"
		"{"
			"return texture(sampler, coordinate).r;"
		"}");

	// Show only the centre 75% of the TV frame.
	crt_->set_video_signal(Outputs::CRT::VideoSignal::Composite);
	crt_->set_visible_area(Outputs::CRT::Rect(0.115f, 0.117f, 0.77f, 0.77f));
	crt_->set_immediate_default_phase(0.0f);
}

Outputs::CRT::CRT *VideoBase::get_crt() {
	return crt_.get();
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

	// Bytes in the character ROM are stored in reverse bit order. Reverse them
	// ahead of time so as to be able to use the same scaling table as for
	// high-resolution graphics.
	for(auto &byte : character_rom_) {
		byte =
			((byte & 0x40) ? 0x01 : 0x00) |
			((byte & 0x20) ? 0x02 : 0x00) |
			((byte & 0x10) ? 0x04 : 0x00) |
			((byte & 0x08) ? 0x08 : 0x00) |
			((byte & 0x04) ? 0x10 : 0x00) |
			((byte & 0x02) ? 0x20 : 0x00) |
			((byte & 0x01) ? 0x40 : 0x00) |
			(byte & 0x80);
	}
}
