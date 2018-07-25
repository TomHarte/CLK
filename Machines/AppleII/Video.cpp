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
	page2_ = page2;;
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
}
