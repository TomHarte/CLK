//
//  StaticAnalyser.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 23/08/2016.
//  Copyright 2016 Thomas Harte. All rights reserved.
//

#ifndef StaticAnalyser_hpp
#define StaticAnalyser_hpp

#include "../Machines.hpp"

#include "../../Storage/Cartridge/Cartridge.hpp"
#include "../../Storage/Disk/Disk.hpp"
#include "../../Storage/MassStorage/MassStorageDevice.hpp"
#include "../../Storage/Tape/Tape.hpp"

#include <memory>
#include <string>
#include <vector>

namespace Analyser {
namespace Static {

/*!
	A list of disks, tapes and cartridges.
*/
struct Media {
	std::vector<std::shared_ptr<Storage::Disk::Disk>> disks;
	std::vector<std::shared_ptr<Storage::Tape::Tape>> tapes;
	std::vector<std::shared_ptr<Storage::Cartridge::Cartridge>> cartridges;
	std::vector<std::shared_ptr<Storage::MassStorage::MassStorageDevice>> mass_storage_devices;

	bool empty() const {
		return disks.empty() && tapes.empty() && cartridges.empty() && mass_storage_devices.empty();
	}
};

/*!
	A list of disks, tapes and cartridges plus information about the machine to which to attach them and its configuration,
	and instructions on how to launch the software attached, plus a measure of confidence in this target's correctness.
*/
struct Target {
	Target(Machine machine) : machine(machine) {}
	virtual ~Target() {}

	Machine machine;
	Media media;
	float confidence = 0.0f;
};
typedef std::vector<std::unique_ptr<Target>> TargetList;

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
}

#endif /* StaticAnalyser_hpp */
