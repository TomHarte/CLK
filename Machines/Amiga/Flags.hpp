//
//  Flags.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 13/10/2021.
//  Copyright © 2021 Thomas Harte. All rights reserved.
//

#ifndef Flags_hpp
#define Flags_hpp

namespace Amiga {

enum class InterruptFlag: uint16_t {
	SerialPortTransmit		= 1 << 0,
	DiskBlock				= 1 << 1,
	Software				= 1 << 2,
	IOPortsAndTimers		= 1 << 3,	// i.e. CIA A.
	Copper					= 1 << 4,
	VerticalBlank			= 1 << 5,
	Blitter					= 1 << 6,
	AudioChannel0			= 1 << 7,
	AudioChannel1			= 1 << 8,
	AudioChannel2			= 1 << 9,
	AudioChannel3			= 1 << 10,
	SerialPortReceive		= 1 << 11,
	DiskSyncMatch			= 1 << 12,
	External				= 1 << 13,	// i.e. CIA B.
};

};

#endif /* Flags_hpp */
