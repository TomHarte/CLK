//
//  Video.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 06/06/2017.
//  Copyright © 2017 Thomas Harte. All rights reserved.
//

#include "Video.hpp"

using namespace ZX8081;

Video::Video() :
	crt_(new Outputs::CRT::CRT(210 * 2, 1, Outputs::CRT::DisplayType::PAL50, 1)),
	line_data_(nullptr),
	line_data_pointer_(nullptr),
	cycles_since_update_(0),
	sync_(false) {

	crt_->set_composite_sampling_function(
		"float composite_sample(usampler2D sampler, vec2 coordinate, vec2 icoordinate, float phase, float amplitude)"
		"{"
			"return float(texture(texID, coordinate).r) / 255.0;"
		"}");
}

void Video::run_for_cycles(int number_of_cycles) {
	cycles_since_update_ += (unsigned int)number_of_cycles << 1;
}

void Video::flush() {
	if(sync_) {
		crt_->output_sync(cycles_since_update_);
	} else {
		if(line_data_) {
			unsigned int data_length = (unsigned int)(line_data_pointer_ - line_data_);
			if(data_length < cycles_since_update_) {
				crt_->output_data(data_length, 1);
				line_data_pointer_ = line_data_ = nullptr;
				cycles_since_update_ -= data_length;
			} else return;
		}

		uint8_t *colour_pointer = (uint8_t *)crt_->allocate_write_area(1);
		if(colour_pointer) *colour_pointer = 0xff;
		crt_->output_level(cycles_since_update_);
	}

	cycles_since_update_ = 0;
}

void Video::set_sync(bool sync) {
	if(sync_ == sync) return;
	flush();
	sync_ = sync;
	if(cycles_since_update_) flush();
}

void Video::output_byte(uint8_t byte) {
	flush();

	if(!line_data_) {
		line_data_pointer_ = line_data_ = crt_->allocate_write_area(320);
	}

	if(line_data_) {
		uint8_t mask = 0x80;
		for(int c = 0; c < 8; c++) {
			line_data_pointer_[c] = (byte & mask) ? 0xff : 0x00;
			mask >>= 1;
		}
		line_data_pointer_ += 8;

		if(line_data_pointer_ - line_data_ == 320) {
			crt_->output_data(320, 1);
			line_data_pointer_ = line_data_ = nullptr;
			cycles_since_update_ -= 160;
		}
	}
}

std::shared_ptr<Outputs::CRT::CRT> Video::get_crt() {
	return crt_;
}
