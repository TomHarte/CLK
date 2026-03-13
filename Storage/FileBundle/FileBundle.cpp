//
//  FileBundle.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 19/11/2025.
//  Copyright © 2025 Thomas Harte. All rights reserved.
//

#include "FileBundle.hpp"

#include <cstdio>
#include <sys/stat.h>

using namespace Storage::FileBundle;

LocalFSFileBundle::LocalFSFileBundle(const std::string_view to_contain) {
	struct stat stats;
	stat(std::string(to_contain).c_str(), &stats);

	if(S_ISDIR(stats.st_mode)) {
		set_base_path(to_contain);
	} else {
		const auto last_separator = to_contain.find_last_of("/\\");
		if(last_separator == std::string::npos) {
			key_file_ = to_contain;
		} else {
			base_path_ = to_contain.substr(0, last_separator + 1);
			key_file_ = to_contain.substr(last_separator + 1);
		}
	}
}

std::optional<std::string> LocalFSFileBundle::key_file() const {
	if(key_file_.empty()) {
		return std::nullopt;
	}
	return key_file_;
}

std::optional<std::string> LocalFSFileBundle::base_path() const {
	return base_path_;
}

void LocalFSFileBundle::set_base_path(const std::string_view path) {
	base_path_ = path;
	if(base_path_.back() != '/') {
		base_path_ += '/';
	}
}

void LocalFSFileBundle::set_permission_delegate(PermissionDelegate *const delegate) {
	permission_delegate_ = delegate;
}

Storage::FileHolder LocalFSFileBundle::open(const std::string_view name, const Storage::FileMode mode) {
	std::string full_path = base_path_;
	full_path += name;

	if(permission_delegate_) {
		permission_delegate_->validate_open(*this, full_path, mode);
	}
	return Storage::FileHolder(full_path, mode);
}

bool LocalFSFileBundle::erase(const std::string_view name) {
	std::string full_path = base_path_;
	full_path += name;

	if(permission_delegate_) {
		permission_delegate_->validate_erase(*this, full_path);
	}
	return !remove(full_path.c_str());
}
