//
//  CartridgeMNetwork.h
//  Clock Signal
//
//  Created by Thomas Harte on 18/03/2017.
//  Copyright 2017 Thomas Harte. All rights reserved.
//

#ifndef Atari2600_CartridgeMNetwork_hpp
#define Atari2600_CartridgeMNetwork_hpp

#include "Cartridge.hpp"

namespace Atari2600 {
namespace Cartridge {

class MNetwork: public BusExtender {
	public:
		MNetwork(uint8_t *rom_base, std::size_t rom_size) :
			BusExtender(rom_base, rom_size) {
			rom_ptr_[0] = rom_base + rom_size_ - 4096;
			rom_ptr_[1] = rom_ptr_[0] + 2048;
			high_ram_ptr_ = high_ram_;
		}

		void perform_bus_operation(CPU::MOS6502::BusOperation operation, uint16_t address, uint8_t *value) {
			address &= 0x1fff;
			if(!(address & 0x1000)) return;

			if(address >= 0x1fe0 && address <= 0x1fe6) {
				rom_ptr_[0] = rom_base_ + (address - 0x1fe0) * 2048;
			} else if(address == 0x1fe7) {
				rom_ptr_[0] = nullptr;
			} else if(address >= 0x1ff8 && address <= 0x1ffb) {
				int offset = (address - 0x1ff8) * 256;
				high_ram_ptr_ = &high_ram_[offset];
			}

			if(address & 0x800) {
				if(address < 0x1900) {
					high_ram_ptr_[address & 255] = *value;
				} else if(address < 0x1a00) {
					if(isReadOperation(operation)) *value = high_ram_ptr_[address & 255];
				} else {
					if(isReadOperation(operation)) *value = rom_ptr_[1][address & 2047];
				}
			} else {
				if(rom_ptr_[0]) {
					if(isReadOperation(operation)) *value = rom_ptr_[0][address & 2047];
				} else {
					if(address < 0x1400) {
						low_ram_[address & 1023] = *value;
					} else {
						if(isReadOperation(operation)) *value = low_ram_[address & 1023];
					}
				}
			}
		}

	private:
		uint8_t *rom_ptr_[2];
		uint8_t *high_ram_ptr_;
		uint8_t low_ram_[1024], high_ram_[1024];
};

}
}

#endif /* Atari2600_CartridgeMNetwork_hpp */
