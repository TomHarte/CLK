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
			featcher_address_{0, 0, 0, 0, 0, 0, 0, 0},
			mask_{0, 0, 0, 0, 0, 0, 0, 0} {
			rom_ptr_ = rom_.data();
		}

		void perform_bus_operation(CPU6502::BusOperation operation, uint16_t address, uint8_t *value) {
			address &= 0x1fff;
			if(!(address & 0x1000)) return;

			switch(address) {

#pragma mark - Reads

				// The random number generator
				case 0x1000: case 0x1001: case 0x1002: case 0x1003: case 0x1004:
					if(isReadOperation(operation)) {
						*value = random_number_generator_;
					}
					random_number_generator_ = (uint8_t)(
						(random_number_generator_ << 1) |
						(~(	(random_number_generator_ >> 7) ^
							(random_number_generator_ >> 5) ^
							(random_number_generator_ >> 4) ^
							(random_number_generator_ >> 3)
						) & 1));
				break;

				// Music fetcher
				case 0x1005: case 0x1006: case 0x1007:
					*value = 0x00;
				break;

				case 0x1008: case 0x1009: case 0x100a: case 0x100b: case 0x100c: case 0x100d: case 0x100e: case 0x100f:
					*value = rom_[8192 + address_for_counter(address & 7)];
				break;

				case 0x1010: case 0x1011: case 0x1012: case 0x1013: case 0x1014: case 0x1015: case 0x1016: case 0x1017: {
					uint8_t mask = mask_[address & 7];
					*value = rom_[8192 + address_for_counter(address & 7)] & mask;
				} break;

#pragma mark - Writes

				case 0x1040: case 0x1041: case 0x1042: case 0x1043: case 0x1044: case 0x1045: case 0x1046: case 0x1047:
					top_[address & 7] = *value;
				break;
				case 0x1048: case 0x1049: case 0x104a: case 0x104b: case 0x104c: case 0x104d: case 0x104e: case 0x104f:
					bottom_[address & 7] = *value;
				break;
				case 0x1050: case 0x1051: case 0x1052: case 0x1053: case 0x1054: case 0x1055: case 0x1056: case 0x1057:
					featcher_address_[address & 7] = (featcher_address_[address & 7] & 0xff00) | *value;
					mask_[address & 7] = 0x00;
				break;
				case 0x1058: case 0x1059: case 0x105a: case 0x105b: case 0x105c: case 0x105d: case 0x105e: case 0x105f:
					featcher_address_[address & 7] = (featcher_address_[address & 7] & 0x00ff) | (uint16_t)(*value << 8);
				break;
				case 0x1070: case 0x1071: case 0x1072: case 0x1073: case 0x1074: case 0x1075: case 0x1076: case 0x1077:
					random_number_generator_ = 0;
				break;

#pragma mark - Paging

				case 0x1ff8: rom_ptr_ = rom_.data();		break;
				case 0x1ff9: rom_ptr_ = rom_.data() + 4096;	break;

#pragma mark - Business as usual

				default:
					if(isReadOperation(operation)) {
						*value = rom_ptr_[address & 4095];
					}
				break;
			}
		}

	private:
		inline uint16_t address_for_counter(int counter) {
			uint16_t fetch_address = (featcher_address_[counter] & 2047) ^ 2047;
			featcher_address_[counter]--;
			if((featcher_address_[counter] & 0xff) == top_[counter]) mask_[counter] = 0xff;
			if((featcher_address_[counter] & 0xff) == bottom_[counter]) mask_[counter] = 0x00;
			return fetch_address;
		}

		uint16_t featcher_address_[8];
		uint8_t top_[8], bottom_[8], mask_[8];
		uint8_t music_mode_[3];
		uint8_t random_number_generator_;
		uint8_t *rom_ptr_;
};

}

#endif /* Atari2600_CartridgePitfall2_hpp */
