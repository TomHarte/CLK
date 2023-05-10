//
//  BitReverse.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 03/10/2017.
//  Copyright 2017 Thomas Harte. All rights reserved.
//

#ifndef BitReverse_hpp
#define BitReverse_hpp

#include <cstdint>
#include <vector>

namespace Storage::Data::BitReverse {

/*!
	Reverses the order of the bits in every byte of the vector:
	bit 7 exchanges with bit 0, bit 6 exchanges with bit 1, 5 with 2,
	and 4 with 3.
*/
void reverse(std::vector<uint8_t> &vector);

// TODO: is this substantially different from Numeric/BitReverse.hpp?

}

#endif /* BitReverse_hpp */
