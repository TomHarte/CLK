//
//  StaticAnalyser.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 24/06/2021.
//  Copyright 2021 Thomas Harte. All rights reserved.
//

#include "StaticAnalyser.hpp"
#include "Target.hpp"

#include "../../../Storage/Disk/Parsers/FAT.hpp"

#include <algorithm>

namespace {

bool insensitive_equal(const std::string &lhs, const std::string &rhs) {
	return std::equal(
		lhs.begin(), lhs.end(),
		rhs.begin(), rhs.end(),
		[] (char l, char r) {
			return tolower(l) == tolower(r);
	});
}

}

Analyser::Static::TargetList Analyser::Static::Enterprise::GetTargets(const Media &media, const std::string &, TargetPlatform::IntType) {
	// This analyser can comprehend disks only.
	if(media.disks.empty()) return {};

	// Otherwise, assume a return will happen.
	Analyser::Static::TargetList targets;
	using Target = Analyser::Static::Enterprise::Target;
	auto *const target = new Target;
	target->media = media;

	// Always require a BASIC.
	target->basic_version = Target::BASICVersion::Any;

	// Inspect any supplied disks.
	if(!media.disks.empty()) {
		// DOS will be needed.
		target->dos = Target::DOS::EXDOS;

		// Grab the volume information, which includes the root directory.
		auto volume = Storage::Disk::FAT::GetVolume(media.disks.front());
		if(volume) {
			// If there's an EXDOS.INI then this disk should be able to boot itself.
			// If not but if there's only one .COM or .BAS, automatically load that.
			// Failing that, issue a :DIR and give the user a clue as to how to load.
			const Storage::Disk::FAT::File *selected_file = nullptr;
			bool has_exdos_ini = false;
			bool did_pick_file = false;
			for(const auto &file: (*volume).root_directory) {
				if(insensitive_equal(file.name, "EXDOS   ") && insensitive_equal(file.extension, "INI")) {
					has_exdos_ini = true;
					break;
				}

				if(insensitive_equal(file.extension, "com") || insensitive_equal(file.extension, "bas")) {
					did_pick_file = !selected_file;
					selected_file = &file;
				}
			}

			if(!has_exdos_ini) {
				if(did_pick_file) {
					target->loading_command = std::string("run \"") + selected_file->name + "." + selected_file->extension + "\"";
				} else {
					target->loading_command = ":dir";
				}
			}
		}
	}

	targets.push_back(std::unique_ptr<Analyser::Static::Target>(target));

	return targets;
}
