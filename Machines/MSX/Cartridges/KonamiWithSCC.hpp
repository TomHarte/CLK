//
//  KonamiWithSCC.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 04/01/2018.
//  Copyright © 2018 Thomas Harte. All rights reserved.
//

#ifndef KonamiWithSCC_hpp
#define KonamiWithSCC_hpp

#include "../ROMSlotHandler.hpp"

namespace MSX {
namespace Cartridge {

class KonamiWithSCCROMSlotHandler: public ROMSlotHandler {
	public:
		KonamiWithSCCROMSlotHandler(MSX::MemoryMap &map, int slot) :
			map_(map), slot_(slot) {}

		void write(uint16_t address, uint8_t value) {
			switch(address >> 11) {
				default: break;
				case 0x0a:
					map_.map(slot_, value * 8192, 0x4000, 0x2000);
				break;
				case 0x0e:
					map_.map(slot_, value * 8192, 0x6000, 0x2000);
				break;
				case 0x12:
					map_.map(slot_, value * 8192, 0x8000, 0x2000);
				break;
				case 0x16:
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

#endif /* KonamiWithSCC_hpp */
