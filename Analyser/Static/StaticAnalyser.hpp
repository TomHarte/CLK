//
//  StaticAnalyser.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 23/08/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#ifndef StaticAnalyser_hpp
#define StaticAnalyser_hpp

#include "../Machines.hpp"

#include "../../Storage/Tape/Tape.hpp"
#include "../../Storage/Disk/Disk.hpp"
#include "../../Storage/Cartridge/Cartridge.hpp"

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

	bool empty() const {
		return disks.empty() && tapes.empty() && cartridges.empty();
	}
};

/*!
	A list of disks, tapes and cartridges plus information about the machine to which to attach them and its configuration,
	and instructions on how to launch the software attached, plus a measure of confidence in this target's correctness.
*/
struct Target {
	virtual ~Target() {}

	Machine machine;
	Media media;

	float confidence;
	std::string loading_command;
};

/*!
	Attempts, through any available means, to return a list of potential targets for the file with the given name.

	@returns The list of potential targets, sorted from most to least probable.
*/
std::vector<std::unique_ptr<Target>> GetTargets(const std::string &file_name);

/*!
	Inspects the supplied file and determines the media included.
*/
Media GetMedia(const std::string &file_name);

}
}

#endif /* StaticAnalyser_hpp */
