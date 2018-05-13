//
//  Tape.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 25/12/2017.
//  Copyright 2017 Thomas Harte. All rights reserved.
//

#ifndef StaticAnalyser_MSX_Tape_hpp
#define StaticAnalyser_MSX_Tape_hpp

#include "../../../Storage/Tape/Tape.hpp"

#include <string>
#include <vector>

namespace Analyser {
namespace Static {
namespace MSX {

struct File {
	std::string name;
	enum Type {
		Binary,
		TokenisedBASIC,
		ASCII
	} type;

	std::vector<uint8_t> data;

	uint16_t starting_address;	// Provided only for Type::Binary files.
	uint16_t entry_address;		// Provided only for Type::Binary files.

	File(File &&rhs);
	File();
};

std::vector<File> GetFiles(const std::shared_ptr<Storage::Tape::Tape> &tape);

}
}
}

#endif /* StaticAnalyser_MSX_Tape_hpp */
