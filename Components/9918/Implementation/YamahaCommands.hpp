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

struct Vector {
	int v[2]{};

	template <int offset, bool high> void set(uint8_t value) {
		constexpr uint8_t mask = high ? (offset ? 0x3 : 0x1) : 0xff;
		constexpr int shift = high ? 8 : 0;
		v[offset] = (v[offset] & ~(mask << shift)) | (value << shift);
	}

	Vector & operator += (const Vector &rhs) {
		v[0] += rhs.v[0];
		v[1] += rhs.v[1];
		return *this;
	}
};

struct CommandContext {
	Vector source;
	Vector destination;
	Vector size;
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
