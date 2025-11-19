//
//  FileBundle.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 19/11/2025.
//  Copyright © 2025 Thomas Harte. All rights reserved.
//

#pragma once

#include "Storage/FileHolder.hpp"
#include <string>

namespace Storage::FileBundle {

/*!
	A File Bundle is a collection of individual files, abstracted from whatever media they might
	be one.

	Initial motivation is allowing some machines direct local filesystem access. An attempt has
	been made to draft this in such a way as to allow it to do things like expose ZIP files as
	bundles in the future.
*/
struct FileBundle {
	virtual FileHolder key_file() = 0;
	virtual uint32_t open(const std::string &) = 0;
};


struct LocalFSFileBundle: public FileBundle {
	LocalFSFileBundle(const std::string &to_contain) : to_contain_(to_contain) {}

	FileHolder key_file() override {
		return FileHolder(to_contain_);
	}

	uint32_t open(const std::string &) override {
		return 0;
	}

private:
	std::string to_contain_;
};

};
