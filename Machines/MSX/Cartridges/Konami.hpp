//
//  Konami.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 04/01/2018.
//  Copyright 2018 Thomas Harte. All rights reserved.
//

#ifndef Konami_hpp
#define Konami_hpp

#include "../MemorySlotHandler.hpp"

namespace MSX {
namespace Cartridge {

class KonamiROMSlotHandler: public MemorySlotHandler {
	public:
		KonamiROMSlotHandler(MSX::MemorySlot &slot) : slot_(slot) {}

		void write(uint16_t address, uint8_t value, bool pc_is_outside_bios) final {
			switch(address >> 13) {
				default:
					if(pc_is_outside_bios) confidence_counter_.add_miss();
				break;
				case 3:
					if(pc_is_outside_bios) {
						if(address == 0x6000) confidence_counter_.add_hit(); else confidence_counter_.add_equivocal();
					}
					slot_.map(value * 0x2000, 0x6000, 0x2000);
				break;
				case 4:
					if(pc_is_outside_bios) {
						if(address == 0x8000) confidence_counter_.add_hit(); else confidence_counter_.add_equivocal();
					}
					slot_.map(value * 0x2000, 0x8000, 0x2000);
				break;
				case 5:
					if(pc_is_outside_bios) {
						if(address == 0xa000) confidence_counter_.add_hit(); else confidence_counter_.add_equivocal();
					}
					slot_.map(value * 0x2000, 0xa000, 0x2000);
				break;
			}
		}

		virtual std::string debug_type() final {
			return "K";
		}
	private:
		MSX::MemorySlot &slot_;
};

}
}

#endif /* Konami_hpp */
