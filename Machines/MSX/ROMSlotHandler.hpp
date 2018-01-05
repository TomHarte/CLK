//
//  ROMSlotHandler.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 03/01/2018.
//  Copyright © 2018 Thomas Harte. All rights reserved.
//

#ifndef ROMSlotHandler_hpp
#define ROMSlotHandler_hpp

#include "../../ClockReceiver/ClockReceiver.hpp"

#include <cstddef>
#include <cstdint>

/*
	Design assumption in this file: to-ROM writes and paging events are 'rare',
	so virtual call costs aren't worrisome.
*/
namespace MSX {

class MemoryMap {
	public:
		virtual void map(int slot, std::size_t source_address, uint16_t destination_address, std::size_t length) = 0;
};

class ROMSlotHandler {
	public:
		virtual void run_for(HalfCycles half_cycles) {}
		virtual void write(uint16_t address, uint8_t value) = 0;
};

}

#endif /* ROMSlotHandler_hpp */
