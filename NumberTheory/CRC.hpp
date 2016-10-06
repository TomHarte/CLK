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
			reset_value_(reset_value), value_(reset_value), polynomial_(polynomial) {}

		inline void reset() { value_ = reset_value_; }
		inline void add(uint8_t value) {
			// TODO: go table based
			value_ ^= (uint16_t)value << 8;
			for(int c = 0; c < 8; c++)
			{
				uint16_t exclusive_or = (value_&0x8000) ? polynomial_ : 0x0000;
				value_ = (uint16_t)(value_ << 1) ^ exclusive_or;
			}
		}
		inline uint16_t get_value() {	return value_; }

	private:
		uint16_t reset_value_, polynomial_;
		uint16_t value_;
};

}

#endif /* CRC_hpp */
