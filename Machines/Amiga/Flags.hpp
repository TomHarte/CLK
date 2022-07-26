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

namespace InterruptFlag {
	using FlagT = uint16_t;

	constexpr FlagT SerialPortTransmit	= 1 << 0;
	constexpr FlagT DiskBlock			= 1 << 1;
	constexpr FlagT Software			= 1 << 2;
	constexpr FlagT IOPortsAndTimers	= 1 << 3;	// i.e. CIA A.
	constexpr FlagT Copper				= 1 << 4;
	constexpr FlagT VerticalBlank		= 1 << 5;
	constexpr FlagT Blitter				= 1 << 6;
	constexpr FlagT AudioChannel0		= 1 << 7;
	constexpr FlagT AudioChannel1		= 1 << 8;
	constexpr FlagT AudioChannel2		= 1 << 9;
	constexpr FlagT AudioChannel3		= 1 << 10;
	constexpr FlagT SerialPortReceive	= 1 << 11;
	constexpr FlagT DiskSyncMatch		= 1 << 12;
	constexpr FlagT External			= 1 << 13;	// i.e. CIA B.
}

namespace DMAFlag {
	using FlagT = uint16_t;

	constexpr FlagT AudioChannel0		= 1 << 0;
	constexpr FlagT AudioChannel1		= 1 << 1;
	constexpr FlagT AudioChannel2		= 1 << 2;
	constexpr FlagT AudioChannel3		= 1 << 3;
	constexpr FlagT Disk				= 1 << 4;
	constexpr FlagT Sprites				= 1 << 5;
	constexpr FlagT Blitter				= 1 << 6;
	constexpr FlagT Copper				= 1 << 7;
	constexpr FlagT Bitplane			= 1 << 8;
	constexpr FlagT AllBelow			= 1 << 9;
	constexpr FlagT BlitterPriority		= 1 << 10;
	constexpr FlagT BlitterZero			= 1 << 13;
	constexpr FlagT BlitterBusy			= 1 << 14;
}

}

#endif /* Flags_hpp */
