//
//  YamahaCommands.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 26/01/2023.
//  Copyright © 2023 Thomas Harte. All rights reserved.
//

#ifndef YamahaCommands_hpp
#define YamahaCommands_hpp

#include "AccessEnums.hpp"

namespace TI {
namespace TMS {

struct CommandContext {
	int source_x = 0, source_y = 0;
	int destination_x = 0, destination_y = 0;
	int size_x = 0, size_y = 0;
	uint8_t colour = 0;
	uint8_t arguments = 0;
};

struct Command {
	// In net:
	//
	// This command is blocked until @c access has been performed, reading
	// from or writing to @c value. It should not be performed until at least
	// @c cycles have passed.
	MemoryAccess access = MemoryAccess::None;
	int cycles = 0;
	uint8_t value = 0;

	/// Current command parameters.
	CommandContext &context;
	Command(CommandContext &context) : context(context) {}

	/// Request that the fields above are updated given the completed access.
	virtual bool next() = 0;
};

}
}

#endif /* YamahaCommands_hpp */
