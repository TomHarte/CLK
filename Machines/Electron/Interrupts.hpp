//
//  Interrupts.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 03/12/2016.
//  Copyright 2016 Thomas Harte. All rights reserved.
//

#pragma once

#include <cstdint>

namespace Electron {

enum Interrupt: uint8_t {
	PowerOnReset		= 0x02,
	DisplayEnd			= 0x04,
	RealTimeClock		= 0x08,
	ReceiveDataFull		= 0x10,
	TransmitDataEmpty	= 0x20,
	HighToneDetect		= 0x40
};

}
