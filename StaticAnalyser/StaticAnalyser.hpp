//
//  StaticAnalyser.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 23/08/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#ifndef StaticAnalyser_hpp
#define StaticAnalyser_hpp

#include "../Storage/Disk/Disk.hpp"
#include "../Storage/Tape/Tape.hpp"
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
		enum class Electron {
			TypeCommand,
			HoldShift
		} Electron;
		enum class Vic20 {
			TypeCommand,
		} Vic20;
	} LoadingMethod;
};

std::list<Target> GetTargets(std::shared_ptr<Storage::Disk> disk, std::shared_ptr<Storage::Tape> tape, std::shared_ptr<std::vector<uint8_t>> rom);

}

#endif /* StaticAnalyser_hpp */
