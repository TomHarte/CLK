//
//  FileBundle.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 19/11/2025.
//  Copyright © 2025 Thomas Harte. All rights reserved.
//

#include "FileBundle.hpp"

#include <cstdio>

using namespace Storage::FileBundle;

LocalFSFileBundle::LocalFSFileBundle(const std::string &to_contain) {
	const auto last_separator = to_contain.find_last_of("/\\");
	if(last_separator == std::string::npos) {
		key_file_ = to_contain;
	} else {
		base_path_ = to_contain.substr(0, last_separator + 1);
		key_file_ = to_contain.substr(last_separator + 1);
	}
}

std::optional<std::string> LocalFSFileBundle::key_file() const {
	return key_file_;
}

std::optional<std::string> LocalFSFileBundle::base_path() const {
	return base_path_;
}

void LocalFSFileBundle::set_base_path(const std::string &path) {
	base_path_ = path;
	if(base_path_.back() != '/') {
		base_path_ += '/';
	}
}

void LocalFSFileBundle::set_permission_delegate(PermissionDelegate *const delegate) {
	permission_delegate_ = delegate;
}

Storage::FileHolder LocalFSFileBundle::open(const std::string &name, const Storage::FileMode mode) {
	if(permission_delegate_) {
		permission_delegate_->validate_open(*this, base_path_ + name, mode);
	}
	return Storage::FileHolder(base_path_ + name, mode);
}

bool LocalFSFileBundle::erase(const std::string &name) {
	if(permission_delegate_) {
		permission_delegate_->validate_erase(*this, base_path_ + name);
	}
	return !remove((base_path_ + name).c_str());
}
