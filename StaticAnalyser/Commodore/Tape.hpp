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
	uint16_t starting_address;
	uint16_t ending_address;
	enum {
		Program,
		Stream
	} Type;
	std::vector<uint8_t> data;
};

}
}
#endif /* Tape_hpp */
