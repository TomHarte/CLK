//
//  Konami.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 04/01/2018.
//  Copyright 2018 Thomas Harte. All rights reserved.
//

#ifndef Konami_hpp
#define Konami_hpp

#include "../ROMSlotHandler.hpp"

namespace MSX {
namespace Cartridge {

class KonamiROMSlotHandler: public ROMSlotHandler {
	public:
		KonamiROMSlotHandler(MSX::MemoryMap &map, int slot) :
			map_(map), slot_(slot) {}

		void write(uint16_t address, uint8_t value, bool pc_is_outside_bios) final {
			switch(address >> 13) {
				default:
					if(pc_is_outside_bios) confidence_counter_.add_miss();
				break;
				case 3:
					if(pc_is_outside_bios) {
						if(address == 0x6000) confidence_counter_.add_hit(); else confidence_counter_.add_equivocal();
					}
					map_.map(slot_, value * 0x2000, 0x6000, 0x2000);
				break;
				case 4:
					if(pc_is_outside_bios) {
						if(address == 0x8000) confidence_counter_.add_hit(); else confidence_counter_.add_equivocal();
					}
					map_.map(slot_, value * 0x2000, 0x8000, 0x2000);
				break;
				case 5:
					if(pc_is_outside_bios) {
						if(address == 0xa000) confidence_counter_.add_hit(); else confidence_counter_.add_equivocal();
					}
					map_.map(slot_, value * 0x2000, 0xa000, 0x2000);
				break;
			}
		}

		virtual std::string debug_type() final {
			return "K";
		}
	private:
		MSX::MemoryMap &map_;
		int slot_;
};

}
}

#endif /* Konami_hpp */
