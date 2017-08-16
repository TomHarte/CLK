//
//  CartridgeAtari8k.h
//  Clock Signal
//
//  Created by Thomas Harte on 18/03/2017.
//  Copyright © 2017 Thomas Harte. All rights reserved.
//

#ifndef Atari2600_CartridgeAtari16k_hpp
#define Atari2600_CartridgeAtari16k_hpp

#include "Cartridge.hpp"

namespace Atari2600 {
namespace Cartridge {

class CartridgeAtari16k: public Cartridge<CartridgeAtari16k> {
	public:
		CartridgeAtari16k(const std::vector<uint8_t> &rom) :
			Cartridge(rom) {
			rom_ptr_ = rom_.data();
		}

		void perform_bus_operation(CPU::MOS6502::BusOperation operation, uint16_t address, uint8_t *value) {
			address &= 0x1fff;
			if(!(address & 0x1000)) return;

			if(address >= 0x1ff6 && address <= 0x1ff9) rom_ptr_ = rom_.data() + (address - 0x1ff6) * 4096;

			if(isReadOperation(operation)) {
				*value = rom_ptr_[address & 4095];
			}
		}

	private:
		uint8_t *rom_ptr_;
};

class CartridgeAtari16kSuperChip: public Cartridge<CartridgeAtari16kSuperChip> {
	public:
		CartridgeAtari16kSuperChip(const std::vector<uint8_t> &rom) :
			Cartridge(rom) {
			rom_ptr_ = rom_.data();
		}

		void perform_bus_operation(CPU::MOS6502::BusOperation operation, uint16_t address, uint8_t *value) {
			address &= 0x1fff;
			if(!(address & 0x1000)) return;

			if(address >= 0x1ff6 && address <= 0x1ff9) rom_ptr_ = rom_.data() + (address - 0x1ff6) * 4096;

			if(isReadOperation(operation)) {
				*value = rom_ptr_[address & 4095];
			}

			if(address < 0x1080) ram_[address & 0x7f] = *value;
			else if(address < 0x1100 && isReadOperation(operation)) *value = ram_[address & 0x7f];
		}

	private:
		uint8_t *rom_ptr_;
		uint8_t ram_[128];
};

}
}

#endif /* Atari2600_CartridgeAtari16k_hpp */
