//
//  StaticAnalyser.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 23/08/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#ifndef StaticAnalyser_hpp
#define StaticAnalyser_hpp

#include "../Storage/Tape/Tape.hpp"
#include "../Storage/Disk/Disk.hpp"
#include <string>
#include <list>
#include <vector>

namespace StaticAnalyser {

enum Machine {
	Atari2600,
	Electron,
	Vic20
};

struct Target {
	Machine machine;
	float probability;

	union {
		enum class Vic20 {
			Unexpanded,
			EightKB,
			ThirtyTwoKB
		} Vic20;
	} MemoryModel;

	union {
		enum class Electron {
			ADFS,
			DFS
		} Electron;
		enum class Vic20 {
			C1540
		} Vic20;
	} ExternalHardware;

	std::string loadingCommand;
	union {
		enum class BBCElectron {
			HoldShift
		} BBCElectron;
	} LoadingMethod;

	std::list<std::shared_ptr<Storage::Disk>> disks;
	std::list<std::shared_ptr<Storage::Tape>> tapes;
	// TODO: ROMs. Probably can't model as raw data, but then how to handle bus complexities?
};

std::list<Target> GetTargets(const char *file_name);

}

#endif /* StaticAnalyser_hpp */
