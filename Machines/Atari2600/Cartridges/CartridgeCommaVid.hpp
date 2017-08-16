//
//  CartridgeCommaVid.h
//  Clock Signal
//
//  Created by Thomas Harte on 18/03/2017.
//  Copyright © 2017 Thomas Harte. All rights reserved.
//

#ifndef Atari2600_CartridgeCommaVid_hpp
#define Atari2600_CartridgeCommaVid_hpp

namespace Atari2600 {
namespace Cartridge {

class CartridgeCommaVid: public Cartridge<CartridgeCommaVid> {
	public:
		CartridgeCommaVid(const std::vector<uint8_t> &rom) :
			Cartridge(rom) {}

		void perform_bus_operation(CPU::MOS6502::BusOperation operation, uint16_t address, uint8_t *value) {
			if(!(address & 0x1000)) return;
			address &= 0x1fff;

			if(address < 0x1400) {
				if(isReadOperation(operation)) *value = ram_[address & 1023];
				return;
			}

			if(address < 0x1800) {
				ram_[address & 1023] = *value;
				return;
			}

			if(isReadOperation(operation)) *value = rom_[address & 2047];
		}

	private:
		uint8_t ram_[1024];
};

}
}

#endif /* Atari2600_CartridgeCommaVid_hpp */
