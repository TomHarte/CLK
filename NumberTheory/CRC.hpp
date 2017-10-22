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

/*! Provides a class capable of accumulating a CRC16 from source data. */
class CRC16 {
	public:
		/*!
			Instantiates a CRC16 that will compute the CRC16 specified by the supplied
			@c polynomial and @c reset_value.
		*/
		CRC16(uint16_t polynomial, uint16_t reset_value) :
				reset_value_(reset_value), value_(reset_value) {
			for(int c = 0; c < 256; c++) {
				uint16_t shift_value = static_cast<uint16_t>(c << 8);
				for(int b = 0; b < 8; b++) {
					uint16_t exclusive_or = (shift_value&0x8000) ? polynomial : 0x0000;
					shift_value = static_cast<uint16_t>(shift_value << 1) ^ exclusive_or;
				}
				xor_table[c] = static_cast<uint16_t>(shift_value);
			}
		}

		/// Resets the CRC to the reset value.
		inline void reset() { value_ = reset_value_; }

		/// Updates the CRC to include @c byte.
		inline void add(uint8_t byte) {
			value_ = static_cast<uint16_t>((value_ << 8) ^ xor_table[(value_ >> 8) ^ byte]);
		}

		/// @returns The current value of the CRC.
		inline uint16_t get_value() const {	return value_; }

		/// Sets the current value of the CRC.
		inline void set_value(uint16_t value) { value_ = value; }

	private:
		const uint16_t reset_value_;
		uint16_t xor_table[256];
		uint16_t value_;
};

}

#endif /* CRC_hpp */
