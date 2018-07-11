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
#include <memory>
#include <string>
#include <vector>

namespace ROMMachine {

/*!
	Defines the signature for a function that must be supplied by the host environment in order to give machines
	a route for fetching any system ROMs they might need.

	The caller will supply the idiomatic name of the machine plus a vector of the names of ROM files that it expects
	to be present. The recevier should return a vector of unique_ptrs that either contain the contents of the
	ROM from @c names that corresponds by index, or else are the nullptr
*/
typedef std::function<std::vector<std::unique_ptr<std::vector<uint8_t>>>(const std::string &machine, const std::vector<std::string> &names)> ROMFetcher;

enum class Error {
	MissingROMs
};

}

#endif /* ROMMachine_h */
