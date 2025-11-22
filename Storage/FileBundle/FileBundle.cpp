//
//  FileBundle.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 19/11/2025.
//  Copyright © 2025 Thomas Harte. All rights reserved.
//

#include "FileBundle.hpp"

using namespace Storage::FileBundle;

std::optional<Storage::FileHolder> LocalFSFileBundle::key_file(const Storage::FileMode mode) {
	return Storage::FileHolder(to_contain_, mode);
}

Storage::FileHolder LocalFSFileBundle::open(const std::string &name, Storage::FileMode mode) {
	// TODO: append path. Just cheat for now.
	(void)name;
	return {/* name */ to_contain_, mode};
}

