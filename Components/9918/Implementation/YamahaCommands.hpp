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
	enum class AccessType {
		/// Plots a single pixel of the current contextual colour at @c location,
		/// which occurs as a read, then a 24-cycle pause, then a write.
		PlotPoint
	};
	AccessType access = AccessType::PlotPoint;
	int cycles = 0;
	Vector location;

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

/// Implements the line command, which is plain-old Bresenham.
///
/// Per Grauw timing is:
///
///	* 88 cycles between every pixel plot;
///	* plus an additional 32 cycles if a step along the minor axis is taken.
struct Line: public Command {
	public:
		Line(CommandContext &context) : Command(context) {
			// Set up Bresenham constants.
			if(abs(context.size.v[0]) > abs(context.size.v[1])) {
				major_.v[0] = context.size.v[0] < 0 ? -1 : 1;
				minor_.v[1] = context.size.v[1] < 0 ? -1 : 1;
				major_.v[1] = minor_.v[0] = 0;

				position_ = abs(context.size.v[1]);
				duration_ = abs(context.size.v[0]);
			} else {
				major_.v[1] = context.size.v[1] < 0 ? -1 : 1;
				minor_.v[0] = context.size.v[0] < 0 ? -1 : 1;
				major_.v[0] = minor_.v[1] = 0;

				position_ = abs(context.size.v[0]);
				duration_ = abs(context.size.v[1]);
			}

			numerator_ = position_ << 1;
			denominator_ = duration_ << 1;

			cycles = 0;
			access = AccessType::PlotPoint;
			location = context.destination;
		}

		bool next() {
			if(!duration_) return false;

			--duration_;
			cycles = 88;
			location += major_;
			position_ -= numerator_;
			if(position_ < 0) {
				cycles += 32;
				location += minor_;
				position_ += denominator_;
			}

			return true;
		}

	private:
		int position_, numerator_, denominator_, duration_;
		Vector major_, minor_;
};

}
}
}

#endif /* YamahaCommands_hpp */
