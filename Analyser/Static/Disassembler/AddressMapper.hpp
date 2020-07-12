//
//  AddressMapper.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 30/12/2017.
//  Copyright 2017 Thomas Harte. All rights reserved.
//

#ifndef AddressMapper_hpp
#define AddressMapper_hpp

#include <functional>

namespace Analyser {
namespace Static {
namespace Disassembler {

/*!
	Provides an address mapper that relocates a chunk of memory so that it starts at
	address @c start_address.
*/
template <typename T> std::function<std::size_t(T)> OffsetMapper(T start_address) {
	return [start_address](T argument) {
		return size_t(argument - start_address);
	};
}

}
}
}

#endif /* AddressMapper_hpp */
