//
//  StaticAnalyser.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 23/08/2016.
//  Copyright 2016 Thomas Harte. All rights reserved.
//

#pragma once

#include "../Machines.hpp"

#include "../../Storage/Cartridge/Cartridge.hpp"
#include "../../Storage/Disk/Disk.hpp"
#include "../../Storage/MassStorage/MassStorageDevice.hpp"
#include "../../Storage/Tape/Tape.hpp"
#include "../../Storage/TargetPlatforms.hpp"
#include "../../Reflection/Struct.hpp"

#include <memory>
#include <string>
#include <vector>

namespace Analyser::Static {

struct State;

/*!
	A list of disks, tapes and cartridges, and possibly a state snapshot.
*/
struct Media {
	std::vector<std::shared_ptr<Storage::Disk::Disk>> disks;
	std::vector<std::shared_ptr<Storage::Tape::Tape>> tapes;
	std::vector<std::shared_ptr<Storage::Cartridge::Cartridge>> cartridges;
	std::vector<std::shared_ptr<Storage::MassStorage::MassStorageDevice>> mass_storage_devices;

	bool empty() const {
		return disks.empty() && tapes.empty() && cartridges.empty() && mass_storage_devices.empty();
	}

	Media &operator +=(const Media &rhs) {
		const auto append = [&](auto &destination, auto &source) {
			destination.insert(destination.end(), source.begin(), source.end());
		};

		append(disks, rhs.disks);
		append(tapes, rhs.tapes);
		append(cartridges, rhs.cartridges);
		append(mass_storage_devices, rhs.mass_storage_devices);

		return *this;
	}
};

/*!
	Describes a machine and possibly its state; conventionally subclassed to add other machine-specific configuration fields and any
	necessary instructions on how to launch any software provided, plus a measure of confidence in this target's correctness.
*/
struct Target {
	Target(Machine machine) : machine(machine) {}
	virtual ~Target() = default;

	// This field is entirely optional.
	std::unique_ptr<Reflection::Struct> state;

	Machine machine;
	Media media;
	float confidence = 0.5f;
};
using TargetList = std::vector<std::unique_ptr<Target>>;

/*!
	Attempts, through any available means, to return a list of potential targets for the file with the given name.

	@returns The list of potential targets, sorted from most to least probable.
*/
TargetList GetTargets(const std::string &file_name);

/*!
	Inspects the supplied file and determines the media included.
*/
Media GetMedia(const std::string &file_name);

}
