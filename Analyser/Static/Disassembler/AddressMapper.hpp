//
//  AddressMapper.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 30/12/2017.
//  Copyright 2017 Thomas Harte. All rights reserved.
//

#pragma once

#include <functional>

namespace Analyser::Static::Disassembler {

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
