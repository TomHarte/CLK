//
//  CartridgeActivisionStack.h
//  Clock Signal
//
//  Created by Thomas Harte on 18/03/2017.
//  Copyright © 2017 Thomas Harte. All rights reserved.
//

#ifndef Atari2600_CartridgeActivisionStack_hpp
#define Atari2600_CartridgeActivisionStack_hpp

namespace Atari2600 {

class CartridgeActivisionStack: public Cartridge<CartridgeActivisionStack> {
	public:
		CartridgeActivisionStack(const std::vector<uint8_t> &rom) :
			Cartridge(rom),
			last_opcode_(0x00) {
			rom_ptr_ = rom_.data();
		}

		void perform_bus_operation(CPU6502::BusOperation operation, uint16_t address, uint8_t *value) {
			if(!(address & 0x1000)) return;

			if(operation == CPU6502::BusOperation::ReadOpcode && (last_opcode_ == 0x20 || last_opcode_ == 0x60)) {
				if(address & 0x2000) {
					rom_ptr_ = rom_.data();
				} else {
					rom_ptr_ = rom_.data() + 4096;
				}
			}

			if(isReadOperation(operation)) {
				*value = rom_ptr_[address & 4095];
			}

			if(operation == CPU6502::BusOperation::ReadOpcode) last_opcode_ = *value;
		}

	private:
		uint8_t *rom_ptr_;
		uint8_t last_opcode_;
};

}

#endif /* Atari2600_CartridgeActivisionStack_hpp */
