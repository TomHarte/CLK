//
//  Video.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 20/03/2026.
//  Copyright © 2026 Thomas Harte. All rights reserved.
//

#include "Video.hpp"

using namespace Thomson::MO5;

namespace {

// Video timing, as far as auto-translate lets me figure it out:
//
//	64 cycles/line;
//	56 lines post signalled vsync, then 200 of video, then 56 more, for 312 total.
//
// Start of vsync is connected to CPU IRQ.
//
// Within a line: ??? Who knows ???
//

static constexpr int CyclesPerLine = 64;
}

Video::Video(const uint8_t *const pixels, const uint8_t *const attributes) :
	pixels_(pixels), attributes_(attributes),
	crt_(
		CyclesPerLine,
		1,
		Outputs::Display::Type::SECAM,
		Outputs::Display::InputDataType::Red4Green4Blue4
	) {
//	crt_.set_fixed_framing([&] {
//		run_for(Cycles(10'000));
//	});
}

void Video::run_for(const Cycles cycles) {

	(void)cycles;
}
