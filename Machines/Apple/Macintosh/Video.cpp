//
//  Video.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 03/05/2019.
//  Copyright © 2019 Thomas Harte. All rights reserved.
//

#include "Video.hpp"

using namespace Apple::Macintosh;

// Re: CRT timings, see the Apple Guide to the Macintosh Hardware Family,
// bottom of page 400:
//
//	"For each scan line, 512 pixels are drawn on the screen ...
//	The horizontal blanking interval takes the time of an additional 192 pixels"
//
// And, at the top of 401:
//
//	"The visible portion of a full-screen display consists of 342 horizontal scan lines...
//	During the vertical blanking interval, the turned-off beam ... traces out an additional 28 scan lines,"
//
Video::Video(uint16_t *ram) :
 	crt_(704, 1, 370, Outputs::Display::ColourSpace::YIQ, 1, 1, 6, false, Outputs::Display::InputDataType::Luminance1) {
}

void Video::set_scan_target(Outputs::Display::ScanTarget *scan_target) {
	crt_.set_scan_target(scan_target);
}
