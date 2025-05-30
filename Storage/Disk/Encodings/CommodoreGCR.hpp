//
//  CommodoreGCR.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 29/07/2016.
//  Copyright 2016 Thomas Harte. All rights reserved.
//

#pragma once

#include "Storage/Storage.hpp"
#include <cstdint>

namespace Storage::Encodings {

namespace CommodoreGCR {
	/*!
		@returns the proportion of a second that each bit of data within the specified @c time_zone
		should idiomatically occupy.
	*/
	Time length_of_a_bit_in_time_zone(unsigned int time_zone);

	/*!
		@returns the five-bit GCR encoding for the low four bits of @c nibble.
	*/
	unsigned int encoding_for_nibble(uint8_t nibble);

	/*!
		@returns the ten-bit GCR encoding for @c byte.
	*/
	unsigned int encoding_for_byte(uint8_t byte);

	/*!
		A block is defined to be four source bytes, which encodes to five GCR bytes.
	*/
	void encode_block(uint8_t *destination, const uint8_t *source);

	/*!
		@returns the four bit nibble for the five-bit GCR @c quintet if a valid GCR value; INT_MAX otherwise.
	*/
	unsigned int decoding_from_quintet(unsigned int quintet);

	/*!
		@returns the byte composed by splitting the dectet into two qintets, decoding each and composing the resulting nibbles.
	*/
	unsigned int decoding_from_dectet(unsigned int dectet);
}

}
