//
//  CSL.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 12/06/2024.
//  Copyright © 2024 Thomas Harte. All rights reserved.
//

#include "CSL.hpp"

#include <fstream>
#include <sstream>
#include <unordered_map>
#include <set>

using namespace Storage::Automation;


struct CSLTest {
	CSLTest() {
		CSL c("/Users/thomasharte/Downloads/Shaker_CSL/MODULE A/SHAKE26A-4.CSL");
	}

};
CSLTest test;

CSL::CSL(const std::string &file_name) {
	// Parse the entire file ahead of time; this isn't necessary but introduces
	// little significant overhead and greatly simplifies my debugging.

	std::ifstream file;
	file.open(file_name);

	using Type = Instruction::Type;
	static const std::unordered_map<std::string, Type> keywords = {
		{"csl_version", Type::Version},
		{"reset", Type::Reset},
		{"crtc_select", Type::CRTCSelect},
		{"disk_insert", Type::DiskInsert},
		{"disk_dir", Type::SetDiskDir},
		{"tape_insert", Type::TapeInsert},
		{"tape_dir", Type::SetTapeDir},
		{"tape_play", Type::TapeInsert},
		{"tape_stop", Type::TapeStop},
		{"tape_rewind", Type::TapeRewind},
		{"snapshot_load", Type::SnapshotLoad},
		{"snapshot_dir", Type::SetSnapshotDir},
		{"key_delay", Type::KeyDelay},
		{"key_output", Type::KeyOutput},
		{"key_from_file", Type::KeyFromFile},
		{"wait", Type::Wait},
		{"wait_driveonoff", Type::WaitDriveOnOff},
		{"wait_ssm0000", Type::WaitSSM0000},
		{"screenshot_name", Type::SetScreenshotName},
		{"screenshot_dir", Type::SetScreenshotDir},
		{"screenshot", Type::Screenshot},
		{"snapshot_name", Type::SetSnapshotDir},
		{"csl_load", Type::LoadCSL},
	};

	for(std::string line; std::getline(file, line); ) {
		// Ignore comments.
		if(line[0] == ';') {
			continue;
		}

		std::istringstream stream(line);
		std::string keyword;
		stream >> keyword;

		const auto key_pair = keywords.find(keyword);
		if(key_pair == keywords.end()) {
			throw InvalidKeyword;
		}

		Instruction instruction;
		instruction.type = key_pair->second;

		const auto require = [&](auto &&target) {
			stream >> target;
			if(!stream.good()) {
				throw InvalidArgument;
			}
		};

		switch(instruction.type) {
			// Keywords with a single string mandatory argument.
			case Type::Version: {
				std::string argument;
				require(argument);
				instruction.argument = argument;
			} break;

			// Keywords with a single number mandatory argument.
			case Type::Wait: {
				uint64_t argument;
				require(argument);
				instruction.argument = argument;
			} break;

			// Miscellaneous:
			case Type::Reset: {
				std::string type;
				stream >> type;
				if(stream.good()) {
					if(type != "soft" && type != "hard") {
						throw InvalidArgument;
					}
					instruction.argument = (type == "soft") ? ResetType::Soft : ResetType::Hard;
				}
			} break;

			case Type::CRTCSelect: {
				std::string type;
				require(type);

				static const std::set<std::string> allowed_types = {
					"0", "1", "1A", "1B", "2", "3", "4",
				};
				if(allowed_types.find(type) == allowed_types.end()) {
					throw InvalidArgument;
				}

				instruction.argument = static_cast<uint64_t>(std::stoi(type));
			} break;

			default:
				printf("");
			break;
		}

		instructions.push_back(std::move(instruction));
	}
}
