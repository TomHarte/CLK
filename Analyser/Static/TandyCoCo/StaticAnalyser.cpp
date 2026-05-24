//
//  StaticAnalyser.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 08/05/2026.
//  Copyright © 2026 Thomas Harte. All rights reserved.
//

#include "StaticAnalyser.hpp"

#include "Storage/Tape/Parsers/TandyCoCo.hpp"
#include "Storage/Disk/Parsers/TandyCoCo.hpp"

#include "Target.hpp"

Analyser::Static::TargetList Analyser::Static::TandyCoCo::GetTargets(
	const Media &media,
	const std::string &,
	TargetPlatform::IntType,
	bool
) {
	TargetList targets;
	auto target = std::make_unique<Target>();
	target->media = media;

	if(!media.tapes.empty()) {
		using namespace Storage::Tape::TandyCoCo;

		Parser parser;
		auto &tape = media.tapes.front();
		const auto serialiser = tape->serialiser();

		while(!serialiser->is_at_end()) {
			const auto block = parser.block(*serialiser);
			if(!block.has_value()) continue;

			if(!block->type && block->data.size() >= 9) {
				if(!block->data[8]) {
					target->loading_command = L"CLOAD:RUN\n";
				} else {
					target->loading_command = L"CLOADM:EXEC\n";
				}
				break;
			}
		}
	}

	if(!media.disks.empty()) {
		target->has_disk_drive = true;

		auto loadable = [&]() -> std::optional<Storage::Disk::TandyCoCo::DirectoryEntry> {
			const auto maybe_directory = Storage::Disk::TandyCoCo::directory(*media.disks.front());
			if(!maybe_directory.has_value()) {
				return std::nullopt;
			}
			auto directory = *maybe_directory;

			// Remove anything the user can't actually type.
			std::erase_if(directory, [](const auto &entry) {
				if(entry.name == "        ") return true;
				for(const auto c: entry.name) {
					if(c & 0x80) return true;
				}
				return false;
			});

			// Single item: return that.
			if(directory.size() == 1) {
				return directory[0];
			}

			// Failing that, is there a single thing that's BASIC or no BASIC and a single thing that's machine code?
			int basics = 0;
			int machine_languages = 0;
			for(const auto &entry: directory) {
				basics += entry.file_type == Storage::Disk::TandyCoCo::DirectoryEntry::FileType::BASIC;
				machine_languages +=
					entry.file_type == Storage::Disk::TandyCoCo::DirectoryEntry::FileType::MachineLanguage;
			}

			if(basics == 1) {
				for(const auto &entry: directory) {
					if(entry.file_type == Storage::Disk::TandyCoCo::DirectoryEntry::FileType::BASIC) {
						return entry;
					}
				}
			}

			if(!basics && machine_languages == 1) {
				for(const auto &entry: directory) {
					if(entry.file_type == Storage::Disk::TandyCoCo::DirectoryEntry::FileType::MachineLanguage) {
						return entry;
					}
				}
			}

			return std::nullopt;
		} ();

		if(loadable.has_value()) {
			const bool is_basic = loadable->file_type == Storage::Disk::TandyCoCo::DirectoryEntry::FileType::BASIC;

			target->loading_command = is_basic ? L"RUN\"" : L"LOADM\"";
			target->loading_command += std::wstring(loadable->name.begin(), loadable->name.end());
			if(!is_basic) {
				target->loading_command += L"\":EXEC";
			}
			target->loading_command += L"\n";
		} else {
			target->loading_command = L"DIR\n";
		}
	}

	targets.push_back(std::move(target));
	return targets;
}
