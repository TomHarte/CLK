//
//  ROMMachine.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 07/11/2017.
//  Copyright 2017 Thomas Harte. All rights reserved.
//

#ifndef ROMMachine_hpp
#define ROMMachine_hpp

#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "Utility/ROMCatalogue.hpp"

namespace ROMMachine {

/*!
	Defines the signature for a function that must be supplied by the host environment in order to give machines
	a route for fetching any system ROMs they might need.

	The caller will supply a vector of the names of ROM files that it would like to inspect. The recevier should
	return a vector of unique_ptrs that either contain the contents of the ROM from @c names that corresponds by
	index, or else are @c nullptr.
*/
typedef std::function<ROM::Map(const ROM::Request &request)> ROMFetcher;

enum class Error {
	MissingROMs
};

}

#endif /* ROMMachine_h */
