//
//  Tape.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 29/08/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#ifndef StaticAnalyser_Acorn_Tape_hpp
#define StaticAnalyser_Acorn_Tape_hpp

#include <memory>
#include <string>
#include <vector>

#include "../../Storage/Tape/Tape.hpp"

namespace StaticAnalyser {
namespace Acorn {

struct File {
	std::string name;
	uint16_t load_address;
	uint16_t execution_address;
	bool is_protected;
	std::vector<uint8_t> data;
};

std::unique_ptr<File> GetNextFile(const std::shared_ptr<Storage::Tape::Tape> &tape);

}
}

#endif /* Tape_hpp */
