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
#include "../../../Components/KonamiSCC/KonamiSCC.hpp"

namespace MSX {
namespace Cartridge {

class KonamiWithSCCROMSlotHandler: public ROMSlotHandler {
	public:
		KonamiWithSCCROMSlotHandler(MSX::MemoryMap &map, int slot, Konami::SCC &scc) :
			map_(map), slot_(slot), scc_(scc) {}

		void write(uint16_t address, uint8_t value) override {
			switch(address >> 11) {
				default: break;
				case 0x0a:
					map_.map(slot_, value * 8192, 0x4000, 0x2000);
				break;
				case 0x0e:
					map_.map(slot_, value * 8192, 0x6000, 0x2000);
				break;
				case 0x12:
					if((value&0x3f) == 0x3f) {
						scc_is_visible_ = true;
						map_.unmap(slot_, 0x8000, 0x2000);
					} else {
						scc_is_visible_ = false;
						map_.map(slot_, value * 8192, 0x8000, 0x2000);
					}
				break;
				case 0x13:
					if(scc_is_visible_) scc_.write(address, value);
				break;
				case 0x16:
					map_.map(slot_, value * 8192, 0xa000, 0x2000);
				break;
			}
		}

		uint8_t read(uint16_t address) override {
			if(scc_is_visible_ && address >= 0x9800 && address < 0xa000) {
				return scc_.read(address);
			}
			return 0xff;
		}

	private:
		MSX::MemoryMap &map_;
		int slot_;
		Konami::SCC &scc_;
		bool scc_is_visible_ = false;
};

}
}

#endif /* KonamiWithSCC_hpp */
