//
//  Video.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 14/04/2018.
//  Copyright © 2018 Thomas Harte. All rights reserved.
//

#include "Video.hpp"

using namespace AppleII;

Video::Video() :
	crt_(new Outputs::CRT::CRT(65, 1, Outputs::CRT::DisplayType::NTSC60, 1)) {

	// Set a composite sampling function that assumes 1bpp input, and uses just 7 bits per byte.
	crt_->set_composite_sampling_function(
		"float composite_sample(usampler2D sampler, vec2 coordinate, vec2 icoordinate, float phase, float amplitude)"
		"{"
			"uint texValue = texture(sampler, coordinate).r;"
			"texValue <<= int(icoordinate.x * 7) % 6;"
			"return float(texValue & 128u);"
		"}");

	// Show only the centre 80% of the TV frame.
	crt_->set_video_signal(Outputs::CRT::VideoSignal::Composite);
	crt_->set_visible_area(Outputs::CRT::Rect(0.1f, 0.1f, 0.8f, 0.8f));
}

Outputs::CRT::CRT *Video::get_crt() {
	return crt_.get();
}
