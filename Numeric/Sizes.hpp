//
//  Sizes.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 26/01/2021.
//  Copyright © 2021 Thomas Harte. All rights reserved.
//

#ifndef Sizes_h
#define Sizes_h

#include <limits>
#include <type_traits>

/*!
	Maps to the smallest integral type that can contain max_value, from the following options:

	* uint8_t;
	* uint16_t;
	* uint32_t; or
	* uint64_t.
*/
template <uint64_t max_value> struct MinIntTypeValue {
	using type =
		std::conditional_t<
			max_value <= std::numeric_limits<uint8_t>::max(), uint8_t,
			std::conditional_t<
				max_value <= std::numeric_limits<uint16_t>::max(), uint16_t,
				std::conditional_t<
					max_value <= std::numeric_limits<uint32_t>::max(), uint32_t,
					uint64_t
				>
			>
		>;
};

#endif /* Sizes_h */
