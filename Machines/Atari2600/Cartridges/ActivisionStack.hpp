//
//  CartridgeActivisionStack.h
//  Clock Signal
//
//  Created by Thomas Harte on 18/03/2017.
//  Copyright 2017 Thomas Harte. All rights reserved.
//

#ifndef Atari2600_ActivisionStack_hpp
#define Atari2600_ActivisionStack_hpp

namespace Atari2600 {
namespace Cartridge {

class ActivisionStack: public BusExtender {
	public:
		ActivisionStack(uint8_t *rom_base, std::size_t rom_size) :
			BusExtender(rom_base, rom_size),
			rom_ptr_(rom_base),
			last_opcode_(0x00) {}

		void perform_bus_operation(CPU::MOS6502::BusOperation operation, uint16_t address, uint8_t *value) {
			if(!(address & 0x1000)) return;

			// This is a bit of a hack; a real cartridge can't see either the sync or read lines, and can't see
			// address line 13. Instead it looks for a pattern in recent address accesses that would imply an
			// RST or JSR.
			if(operation == CPU::MOS6502::BusOperation::ReadOpcode && (last_opcode_ == 0x20 || last_opcode_ == 0x60)) {
				if(address & 0x2000) {
					rom_ptr_ = rom_base_;
				} else {
					rom_ptr_ = rom_base_ + 4096;
				}
			}

			if(isReadOperation(operation)) {
				*value = rom_ptr_[address & 4095];
			}

			if(operation == CPU::MOS6502::BusOperation::ReadOpcode) last_opcode_ = *value;
		}

	private:
		uint8_t *rom_ptr_;
		uint8_t last_opcode_;
};

}
}

#endif /* Atari2600_CartridgeActivisionStack_hpp */
