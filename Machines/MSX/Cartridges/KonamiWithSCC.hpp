//
//  KonamiWithSCC.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 04/01/2018.
//  Copyright 2018 Thomas Harte. All rights reserved.
//

#ifndef KonamiWithSCC_hpp
#define KonamiWithSCC_hpp

#include "../MemorySlotHandler.hpp"
#include "../../../Components/KonamiSCC/KonamiSCC.hpp"

namespace MSX {
namespace Cartridge {

class KonamiWithSCCROMSlotHandler: public ROMSlotHandler {
	public:
		KonamiWithSCCROMSlotHandler(MSX::MemorySlot &slot, Konami::SCC &scc) :
			slot_(slot), scc_(scc) {}

		void write(uint16_t address, uint8_t value, bool pc_is_outside_bios) final {
			switch(address >> 11) {
				default:
					if(pc_is_outside_bios) confidence_counter_.add_miss();
				break;
				case 0x0a:
					if(pc_is_outside_bios) {
						if(address == 0x5000) confidence_counter_.add_hit(); else confidence_counter_.add_equivocal();
					}
					slot_.map(0, value * 0x2000, 0x4000, 0x2000);
				break;
				case 0x0e:
					if(pc_is_outside_bios) {
						if(address == 0x7000) confidence_counter_.add_hit(); else confidence_counter_.add_equivocal();
					}
					slot_.map(0, value * 0x2000, 0x6000, 0x2000);
				break;
				case 0x12:
					if(pc_is_outside_bios) {
						if(address == 0x9000) confidence_counter_.add_hit(); else confidence_counter_.add_equivocal();
					}
					if((value&0x3f) == 0x3f) {
						scc_is_visible_ = true;
						slot_.unmap(0, 0x8000, 0x2000);
					} else {
						scc_is_visible_ = false;
						slot_.map(0, value * 0x2000, 0x8000, 0x2000);
					}
				break;
				case 0x13:
					if(scc_is_visible_) {
						if(pc_is_outside_bios) confidence_counter_.add_hit();
						scc_.write(address, value);
					} else {
						if(pc_is_outside_bios) confidence_counter_.add_miss();
					}
				break;
				case 0x16:
					if(pc_is_outside_bios) {
						if(address == 0xb000) confidence_counter_.add_hit(); else confidence_counter_.add_equivocal();
					}
					slot_.map(0, value * 0x2000, 0xa000, 0x2000);
				break;
			}
		}

		uint8_t read(uint16_t address) final {
			if(scc_is_visible_ && address >= 0x9800 && address < 0xa000) {
				confidence_counter_.add_hit();
				return scc_.read(address);
			}
			confidence_counter_.add_miss();
			return 0xff;
		}

		virtual std::string debug_type() final {
			return "KSCC";
		}

	private:
		MSX::MemorySlot &slot_;
		Konami::SCC &scc_;
		bool scc_is_visible_ = false;
};

}
}

#endif /* KonamiWithSCC_hpp */
