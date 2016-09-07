//
//  Tape.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 24/08/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#ifndef StaticAnalyser_Commodore_Tape_hpp
#define StaticAnalyser_Commodore_Tape_hpp

#include <cstdint>
#include "../StaticAnalyser.hpp"

namespace StaticAnalyser {
namespace Commodore {

struct File {
	std::wstring name;
	std::vector<uint8_t> raw_name;
	uint16_t starting_address;
	uint16_t ending_address;
	enum {
		RelocatableProgram,
		NonRelocatableProgram,
		DataSequence,
	} type;
	std::vector<uint8_t> data;
};

std::list<File> GetFiles(const std::shared_ptr<Storage::Tape::Tape> &tape);

}
}
#endif /* Tape_hpp */
