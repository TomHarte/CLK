//
//  ASCII8kb.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 04/01/2018.
//  Copyright © 2018 Thomas Harte. All rights reserved.
//

#ifndef ASCII8kb_hpp
#define ASCII8kb_hpp

#include "../ROMSlotHandler.hpp"

namespace MSX {
namespace Cartridge {

class ASCII8kbROMSlotHandler: public ROMSlotHandler {
	public:
		ASCII8kbROMSlotHandler(MSX::MemoryMap &map, int slot) :
			map_(map), slot_(slot) {}

		void write(uint16_t address, uint8_t value) {
			switch(address >> 11) {
				default: break;
				case 0xc:
					map_.map(slot_, value * 8192, 0x4000, 0x2000);
				break;
				case 0xd:
					map_.map(slot_, value * 8192, 0x6000, 0x2000);
				break;
				case 0xe:
					map_.map(slot_, value * 8192, 0x8000, 0x2000);
				break;
				case 0xf:
					map_.map(slot_, value * 8192, 0xa000, 0x2000);
				break;
			}
		}

	private:
		MSX::MemoryMap &map_;
		int slot_;
};

}
}

#endif /* ASCII8kb_hpp */
