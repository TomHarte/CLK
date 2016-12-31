//
//  CRC.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 18/09/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#ifndef CRC_hpp
#define CRC_hpp

#include <cstdint>

namespace NumberTheory {

class CRC16 {
	public:
		CRC16(uint16_t polynomial, uint16_t reset_value) :
			reset_value_(reset_value), value_(reset_value)
		{
			for(int c = 0; c < 256; c++)
			{
				uint16_t shift_value = (uint16_t)(c << 8);
				for(int b = 0; b < 8; b++)
				{
					uint16_t exclusive_or = (shift_value&0x8000) ? polynomial : 0x0000;
					shift_value = (uint16_t)(shift_value << 1) ^ exclusive_or;
				}
				xor_table[c] = (uint16_t)shift_value;
			}
		}

		inline void reset() { value_ = reset_value_; }
		inline void add(uint8_t byte) {
			value_ = (uint16_t)((value_ << 8) ^ xor_table[(value_ >> 8) ^ byte]);
		}
		inline uint16_t get_value() const {	return value_; }
		inline void set_value(uint16_t value) { value_ = value; }

	private:
		const uint16_t reset_value_;
		uint16_t xor_table[256];
		uint16_t value_;
};

}

#endif /* CRC_hpp */
