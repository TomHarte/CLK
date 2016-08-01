//
//  CommodoreGCR.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 29/07/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#ifndef CommodoreGCR_hpp
#define CommodoreGCR_hpp

#include "../../Storage.hpp"
#include <cstdint>

namespace Storage {
namespace Encodings {

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
}
}
}

#endif /* CommodoreGCR_hpp */
