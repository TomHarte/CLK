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

std::optional<std::string> LocalFSFileBundle::key_file() {
	return key_file_;
}

void LocalFSFileBundle::set_permission_delegate(PermissionDelegate *const delegate) {
	permission_delegate_ = delegate;
}

Storage::FileHolder LocalFSFileBundle::open(const std::string &name, const Storage::FileMode mode) {
	const auto full_name = base_path_ + name;
	if(permission_delegate_) {
		permission_delegate_->validate_open(full_name, mode);
	}
	return Storage::FileHolder(full_name, mode);
}

bool LocalFSFileBundle::erase(const std::string &name) {
	const auto full_name = base_path_ + name;
	if(permission_delegate_) {
		permission_delegate_->validate_erase(full_name);
	}
	return !remove((base_path_ + name).c_str());
}

std::optional<std::string> LocalFSFileBundle::base_path() {
	return base_path_;
}
