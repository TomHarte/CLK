//
//  Video.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 20/03/2026.
//  Copyright © 2026 Thomas Harte. All rights reserved.
//

#include "Video.hpp"

using namespace Thomson::MO5;

// Video timing, as far as auto-translate lets me figure it out:
//
//	64 cycles/line;
//	56 lines post signalled vsync, then 200 of video, then 56 more, for 312 total.
//
// Start of vsync is connected to CPU IRQ.
//
// Within a line: ??? Who knows ???
//

void Video::run_for(const Cycles cycles) {

	(void)cycles;
}
