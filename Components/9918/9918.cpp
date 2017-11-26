//
//  9918.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 25/11/2017.
//  Copyright © 2017 Thomas Harte. All rights reserved.
//

#include "9918.hpp"

using namespace TI;

TMS9918::TMS9918(Personality p) :
	crt_(new Outputs::CRT::CRT(911, 1, Outputs::CRT::DisplayType::NTSC60, 3)) {
	crt_->set_rgb_sampling_function(
		"vec3 rgb_sample(usampler2D sampler, vec2 coordinate, vec2 icoordinate)"
		"{"
			"return texture(sampler, coordinate).rgb;"
		"}");
}

std::shared_ptr<Outputs::CRT::CRT> TMS9918::get_crt() {
	return crt_;
}

void TMS9918::run_for(const Cycles cycles) {
}

void TMS9918::set_register(int address, uint8_t value) {
}

uint8_t TMS9918::get_register(int address) {
	return 0;
}
