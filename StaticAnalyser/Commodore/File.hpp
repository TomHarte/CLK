//
//  File.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 10/09/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#ifndef File_hpp
#define File_hpp

#include <string>
#include <vector>

namespace StaticAnalyser {
namespace Commodore {

struct File {
	std::wstring name;
	std::vector<uint8_t> raw_name;
	uint16_t starting_address;
	uint16_t ending_address;
	bool is_locked = false;
	bool is_closed = false;
	enum {
		RelocatableProgram,
		NonRelocatableProgram,
		DataSequence,
		User,
		Relative
	} type;
	std::vector<uint8_t> data;

	bool is_basic();
};

}
}

#endif /* File_hpp */
