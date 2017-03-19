//
//  CartridgePitfall2.h
//  Clock Signal
//
//  Created by Thomas Harte on 18/03/2017.
//  Copyright © 2017 Thomas Harte. All rights reserved.
//

#ifndef Atari2600_CartridgePitfall2_hpp
#define Atari2600_CartridgePitfall2_hpp

namespace Atari2600 {

class CartridgePitfall2: public Cartridge<CartridgePitfall2> {
	public:
		CartridgePitfall2(const std::vector<uint8_t> &rom) :
			Cartridge(rom),
			random_number_generator_(0),
			featcher_address_{0, 0, 0, 0, 0, 0, 0, 0} {
			rom_ptr_ = rom_.data();
		}

		void perform_bus_operation(CPU6502::BusOperation operation, uint16_t address, uint8_t *value) {
			address &= 0x1fff;
			if(!(address & 0x1000)) return;

			switch(address) {
				// The random number generator
				case 0x1000: case 0x1001: case 0x1002: case 0x1003:
					if(isReadOperation(operation)) {
						*value = random_number_generator_;
					}
					random_number_generator_ = (uint8_t)(
						(random_number_generator_ << 1) |
						~((	(random_number_generator_ >> 7) ^
							(random_number_generator_ >> 5) ^
							(random_number_generator_ >> 4) ^
							(random_number_generator_ >> 3)
						) & 1));
				break;

				case 0x1040: case 0x1041: case 0x1042: case 0x1043: case 0x1044: case 0x1045: case 0x1046: case 0x1047:
//					featcher_address_[address - 0x1040]
				break;

				case 0x1ff8: rom_ptr_ = rom_.data();		break;
				case 0x1ff9: rom_ptr_ = rom_.data() + 4096;	break;

				default:
					if(isReadOperation(operation)) {
						*value = rom_ptr_[address & 4095];
					}
				break;
			}
		}

	private:
		uint16_t featcher_address_[8];
		uint8_t random_number_generator_;
		uint8_t *rom_ptr_;
};

}

#endif /* Atari2600_CartridgePitfall2_hpp */
