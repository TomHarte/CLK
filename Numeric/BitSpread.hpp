//
//  BitSpread.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 12/10/2021.
//  Copyright © 2021 Thomas Harte. All rights reserved.
//

#ifndef BitSpread_hpp
#define BitSpread_hpp

namespace Numeric {

/// @returns The bits of @c input with a 0 bit inserted between each and
/// keeping the least-significant bit in its original position.
///
/// i.e. if @c input is abcdefgh then the result is 0a0b0c0d0e0f0g0h
constexpr uint16_t spread_bits(uint8_t input) {
	uint16_t result = uint16_t(input);				// 0000 0000 abcd efgh
	result = (result | (result << 4)) & 0x0f0f;		// 0000 abcd 0000 efgh
	result = (result | (result << 2)) & 0x3333;		// 00ab 00cd 00ef 00gh
	return (result | (result << 1)) & 0x5555;		// 0a0b 0c0d 0e0f 0g0h
}

/// Performs the opposite action to @c spread_bits; given the 16-bit input
/// @c abcd @c efgh @c ijkl @c mnop, returns the byte value @c bdfhjlnp
/// i.e. every other bit is retained, keeping the least-significant bit in place.
constexpr uint8_t unspread_bits(uint16_t input) {
	input &= 0x5555;								// 0a0b 0c0d 0e0f 0g0h
	input = (input | (input >> 1)) & 0x3333;		// 00ab 00cd 00ef 00gh
	input = (input | (input >> 2)) & 0x0f0f;		// 0000 abcd 0000 efgh
	return uint8_t(input | (input >> 4));			// 0000 0000 abcd efgh
}

}

#endif /* BitSpread_hpp */
