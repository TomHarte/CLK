//
//  CartridgeAtari8k.h
//  Clock Signal
//
//  Created by Thomas Harte on 18/03/2017.
//  Copyright 2017 Thomas Harte. All rights reserved.
//

#ifndef Atari2600_CartridgeAtari8k_hpp
#define Atari2600_CartridgeAtari8k_hpp

#include "Cartridge.hpp"

namespace Atari2600 {
namespace Cartridge {

class Atari8k: public BusExtender {
	public:
		Atari8k(uint8_t *rom_base, std::size_t rom_size) : BusExtender(rom_base, rom_size), rom_ptr_(rom_base) {}

		void perform_bus_operation(CPU::MOS6502::BusOperation operation, uint16_t address, uint8_t *value) {
			address &= 0x1fff;
			if(!(address & 0x1000)) return;

			if(address == 0x1ff8) rom_ptr_ = rom_base_;
			else if(address == 0x1ff9) rom_ptr_ = rom_base_ + 4096;

			if(isReadOperation(operation)) {
				*value = rom_ptr_[address & 4095];
			}
		}

	private:
		uint8_t *rom_ptr_;
};

class Atari8kSuperChip: public BusExtender {
	public:
		Atari8kSuperChip(uint8_t *rom_base, std::size_t rom_size) : BusExtender(rom_base, rom_size), rom_ptr_(rom_base) {}

		void perform_bus_operation(CPU::MOS6502::BusOperation operation, uint16_t address, uint8_t *value) {
			address &= 0x1fff;
			if(!(address & 0x1000)) return;

			if(address == 0x1ff8) rom_ptr_ = rom_base_;
			if(address == 0x1ff9) rom_ptr_ = rom_base_ + 4096;

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

#endif /* Atari2600_CartridgeAtari8k_hpp */
