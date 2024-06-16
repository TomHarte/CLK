//
//  CSL.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 12/06/2024.
//  Copyright © 2024 Thomas Harte. All rights reserved.
//

#pragma once

#include <optional>
#include <string>
#include <vector>

namespace Storage::Automation {

struct CSL {
	CSL(const std::string &file_name);

	enum Errors {
		InvalidKeyword,
		InvalidArgument,
	};

	private:
		enum ResetType {
			Hard, Soft
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
				SnapshotLoad,
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

			std::variant<std::monostate, ResetType, std::string, uint64_t> argument;
		};

		std::vector<Instruction> instructions;
		std::optional<Instruction> next();
};

}
