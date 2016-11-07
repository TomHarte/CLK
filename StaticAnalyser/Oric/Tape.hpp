//
//  Tape.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 06/11/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#ifndef StaticAnalyser_Oric_Tape_hpp
#define StaticAnalyser_Oric_Tape_hpp

#include "../../Storage/Tape/Tape.hpp"
#include <list>
#include <string>
#include <vector>

namespace StaticAnalyser {
namespace Oric {

struct File {
	std::string name;
	uint16_t starting_address;
	uint16_t ending_address;
	enum ProgramType {
		BASIC,
		MachineCode,
		None
	};
	ProgramType data_type, launch_type;
	std::vector<uint8_t> data;
};

std::list<File> GetFiles(const std::shared_ptr<Storage::Tape::Tape> &tape);

}
}

#endif /* Tape_hpp */
