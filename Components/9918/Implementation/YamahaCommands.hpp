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

// MARK: - Generics.

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

	// TODO: how best to describe access destination? Probably as (x, y) and logical/fast?

	/// Current command parameters.
	CommandContext &context;
	Command(CommandContext &context) : context(context) {}

	/// Request that the fields above are updated given that the previously-request access
	/// was completed.
	///
	/// @returns @c true if another access has been enqueued; @c false if this command is done.
	virtual bool next() = 0;
};

// MARK: - Line drawing.

namespace Commands {

struct Line: public Command {
	public:
		Line(CommandContext &context) : Command(context) {
			// Set up Bresenham constants.
		}

		bool next() {
			// Should implement Bresenham with cadence:
			//
			//	88 cycles before the next read; 24 to write.
			//	Add 32 extra cycles if a minor-axis step occurs.
			return false;
		}

	private:
};

}
}
}

#endif /* YamahaCommands_hpp */
