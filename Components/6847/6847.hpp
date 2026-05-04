//
//  6847.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 04/05/2026.
//  Copyright © 2026 Thomas Harte. All rights reserved.
//

#pragma once

// Notes:
//
// # https://www.acornatom.nl/sites/atomreview/howel/logic/6847_clone.htm:
//
// The master clock was designed to run at 3.579545 MHz, because they are used in NTSC TVs and so are very cheap.
// This is also useful for creating NTSC-compatible colour phase signals. Internally the 6847 divides the clock by 3.5
// to get a frame timing frequency of 1.022727 MHz. This is divided by 64 to get the line rate.
//
// Each line has up to 256 pixels from 32 memory accesses. There are 8 pixels per memory access.
