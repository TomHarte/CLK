//
//  AppleGCR.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 21/04/2018.
//  Copyright © 2018 Thomas Harte. All rights reserved.
//

#ifndef AppleGCR_hpp
#define AppleGCR_hpp

#include <cstdint>

namespace Storage {
namespace Encodings {

namespace AppleGCR {

	/*!
		@returns the eight-bit 13-sector GCR encoding for the low five bits of @c value.
	*/
	unsigned int five_and_three_encoding_for_value(int value);

	/*!
		@returns the eight-bit 16-sector GCR encoding for the low six bits of @c value.
	*/
	unsigned int six_and_two_encoding_for_value(int value);

	/*!
		A block is defined to be five source bytes, which encodes to eight GCR bytes.
	*/
	void encode_five_and_three_block(uint8_t *destination, uint8_t *source);

	/*!
		A block is defined to be three source bytes, which encodes to four GCR bytes.
	*/
	void encode_six_and_two_block(uint8_t *destination, uint8_t *source);

	/*!
		@returns the four bit nibble for the five-bit GCR @c quintet if a valid GCR value; INT_MAX otherwise.
	*/
//	unsigned int decoding_from_quintet(unsigned int quintet);

	/*!
		@returns the byte composed by splitting the dectet into two qintets, decoding each and composing the resulting nibbles.
	*/
//	unsigned int decoding_from_dectet(unsigned int dectet);
}

}
}

#endif /* AppleGCR_hpp */
