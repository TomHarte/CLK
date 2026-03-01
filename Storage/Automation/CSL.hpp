//
//  CSL.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 12/06/2024.
//  Copyright © 2024 Thomas Harte. All rights reserved.
//

#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace Storage::Automation::CSL {

enum Reset {
	Hard, Soft
};
struct DiskInsert {
	int drive = 0;
	std::string file;
};
enum ScreenshotOrSnapshot {
	WaitForVSync, Now,
};
struct KeyDelay {
	uint64_t press_delay;
	uint64_t interpress_delay;
	std::optional<uint64_t> carriage_return_delay;
};
struct KeyEvent {
	bool down;
	uint16_t key;
};

struct Instruction {
	enum class Type {
		Version,
		Reset,
		CRTCSelect,
		LoadCSL,

		DiskInsert,
		SetDiskDir,

		TapeInsert,
		SetTapeDir,
		TapePlay,
		TapeStop,
		TapeRewind,

		SetSnapshotDir,
		LoadSnapshot,
		SetSnapshotName,
		Snapshot,

		KeyDelay,
		KeyOutput,
		KeyFromFile,

		Wait,
		WaitDriveOnOff,
		WaitVsyncOnOff,
		WaitSSM0000,

		SetScreenshotName,
		SetScreenshotDir,
		Screenshot,
	} type;

	std::variant<
		std::monostate,
		DiskInsert,
		Reset,
		ScreenshotOrSnapshot,
		KeyDelay,
		std::string,
		std::vector<KeyEvent>,
		uint64_t
	> argument;
};


enum Errors {
	InvalidKeyword,
	InvalidArgument,
};
std::vector<Instruction> parse(std::string_view file_name);

}
