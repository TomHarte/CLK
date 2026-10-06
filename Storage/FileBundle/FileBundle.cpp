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

LocalFSFileBundle::LocalFSFileBundle(const std::filesystem::path &to_contain) {
	struct stat stats;
	stat(to_contain.c_str(), &stats);

	if(S_ISDIR(stats.st_mode)) {
		set_base_path(to_contain);
	} else {
		set_base_path(to_contain.parent_path());
	}
}

std::optional<std::filesystem::path> LocalFSFileBundle::key_file() const {
	if(key_file_.empty()) {
		return std::nullopt;
	}
	return key_file_;
}

std::optional<std::filesystem::path> LocalFSFileBundle::base_path() const {
	return base_path_;
}

void LocalFSFileBundle::set_base_path(const std::filesystem::path &path) {
	base_path_ = path;
}

void LocalFSFileBundle::set_permission_delegate(PermissionDelegate *const delegate) {
	permission_delegate_ = delegate;
}

Storage::FileHolder LocalFSFileBundle::open(const std::filesystem::path &path, const Storage::FileMode mode) {
	if(permission_delegate_) {
		permission_delegate_->validate_open(*this, base_path_ / path, mode);
	}
	return Storage::FileHolder(base_path_ / path, mode);
}

bool LocalFSFileBundle::erase(const std::filesystem::path &path) {
	if(permission_delegate_) {
		permission_delegate_->validate_erase(*this, base_path_ / path);
	}
	return !remove((base_path_ / path).c_str());
}
