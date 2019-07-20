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
	Describes a ROM image; this term is used in this emulator strictly in the sense of firmware —
	system software that is an inherent part of a machine.
*/
struct ROM {
	/// A descriptive name for this ROM, e.g. "Electron MOS 1.0".
	std::string descriptive_name;
	/// An idiomatic file name for this ROM, e.g. "os10.rom".
	std::string file_name;
	/// The expected size of this ROM in bytes, e.g. 32768.
	size_t size = 0;
	/// CRC32s for all known acceptable copies of this ROM; intended to allow a host platform
	/// to test user-provided ROMs of unknown provenance. **Not** intended to be used
	/// to exclude ROMs where the user's intent is otherwise clear.
	std::vector<uint32_t> crc32s;

	/// This is a temporary constructor provided for transitional purposes.
	ROM(std::string file_name) :
		file_name(file_name) {}

	ROM(std::string descriptive_name, std::string file_name, size_t size, uint32_t crc32) :
		descriptive_name(descriptive_name), file_name(file_name), size(size), crc32s({crc32}) {}
	ROM(std::string descriptive_name, std::string file_name, size_t size, std::initializer_list<uint32_t> crc32s) :
		descriptive_name(descriptive_name), file_name(file_name), size(size), crc32s(crc32s) {}
};

/*!
	Defines the signature for a function that must be supplied by the host environment in order to give machines
	a route for fetching any system ROMs they might need.

	The caller will supply the idiomatic name of the machine plus a vector of the names of ROM files that it expects
	to be present. The recevier should return a vector of unique_ptrs that either contain the contents of the
	ROM from @c names that corresponds by index, or else are the nullptr
*/
typedef std::function<std::vector<std::unique_ptr<std::vector<uint8_t>>>(const std::string &machine, const std::vector<ROM> &roms)> ROMFetcher;

enum class Error {
	MissingROMs
};

}

#endif /* ROMMachine_h */
