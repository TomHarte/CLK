//
//  FileBundle.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 19/11/2025.
//  Copyright © 2025 Thomas Harte. All rights reserved.
//

#pragma once

#include "Storage/FileHolder.hpp"
#include <optional>
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
	struct PermissionDelegate {
		virtual void validate_open(FileBundle &, const std::string &, FileMode) = 0;
		virtual void validate_erase(FileBundle &, const std::string &) = 0;
	};

	virtual std::optional<std::string> key_file() const = 0;
	virtual FileHolder open(const std::string &, FileMode) = 0;
	virtual bool erase(const std::string &) = 0;

	virtual std::optional<std::string> base_path() const { return std::nullopt; }
	virtual void set_base_path(const std::string &) {}
	virtual void set_permission_delegate(PermissionDelegate *) {}
};


struct LocalFSFileBundle: public FileBundle {
	LocalFSFileBundle(const std::string &to_contain);

	std::optional<std::string> key_file() const override;
	FileHolder open(const std::string &, FileMode) override;
	bool erase(const std::string &) override;

	std::optional<std::string> base_path() const override;
	void set_base_path(const std::string &) override;
	void set_permission_delegate(PermissionDelegate *) override;

private:
	std::string key_file_;
	std::string base_path_;
	PermissionDelegate *permission_delegate_ = nullptr;
};

};
