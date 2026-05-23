//
//  TandyCoCo.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 23/05/2026.
//  Copyright © 2026 Thomas Harte. All rights reserved.
//

#pragma once

#include "Storage/Disk/Disk.hpp"

#include <optional>
#include <string>
#include <vector>

namespace Storage::Disk::TandyCoCo {

struct DirectoryEntry {
	std::string name;
	std::string extension;

	enum class FileType {
		BASIC = 0,
		BASICData = 1,
		MachineLanguage = 2,
		TextEditorsource = 3,
	};
	FileType file_type;

	enum class ASCIIFlag {
		BinaryOrCrunchedBASIC = 0,
		ASCII = 0xff,
	};
	ASCIIFlag ascii_flag;

	uint8_t first_granule;
	uint16_t bytes_in_last_granule;
};

std::optional<std::vector<DirectoryEntry>> directory(const Storage::Disk::Disk &disk);
}
