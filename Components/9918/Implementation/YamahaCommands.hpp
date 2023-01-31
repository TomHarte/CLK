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

	template <int offset> void add(int amount) {
		v[offset] += amount;

		if constexpr (offset == 1) {
			v[offset] &= 0x3ff;
		} else {
			v[offset] &= 0x1ff;
		}
	}

	Vector & operator += (const Vector &rhs) {
		add<0>(rhs.v[0]);
		add<1>(rhs.v[1]);
		return *this;
	}
};

struct CommandContext {
	Vector source;
	Vector destination;
	Vector size;

	uint8_t arguments = 0;
	/// Colour as written by the CPU.
	uint8_t colour = 0;
	/// The low four bits of the CPU-written colour, repeated twice.
	uint8_t colour4bpp = 0;
	/// The low two bits of the CPU-written colour, repeated four times.
	uint8_t colour2bpp = 0;

	enum class LogicalOperation {
		Copy			= 0b0000,
		And				= 0b0001,
		Or				= 0b0010,
		Xor				= 0b0011,
		Not				= 0b0100,
	};
	LogicalOperation pixel_operation;
	bool test_source;
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
		PlotPoint,

		/// Blocks until the next CPU write to the colour register.
		WaitForColourReceipt,

		/// Writes an entire byte to the location containing the current @c location.
		WriteByte,
	};
	AccessType access = AccessType::PlotPoint;
	int cycles = 0;
	bool is_cpu_transfer = false;
	Vector location;

	/// Current command parameters.
	CommandContext &context;
	Command(CommandContext &context) : context(context) {}

	/// @returns @c true if all output from this command is done; @c false otherwise.
	virtual bool done() = 0;

	/// Repopulates the fields above with the next action to take.
	virtual void advance() = 0;

	protected:
		template <int axis> void advance_axis() {
			context.destination.add<axis>(context.arguments & (0x4 << axis) ? -1 : 1);
		}

};

// MARK: - Line drawing.

namespace Commands {

/// Implements the LINE command, which is plain-old Bresenham.
///
/// Per Grauw timing is:
///
///	* 88 cycles between every pixel plot;
///	* plus an additional 32 cycles if a step along the minor axis is taken.
struct Line: public Command {
	public:
		Line(CommandContext &context) : Command(context) {
			// context.destination = start position;
			// context.size.v[0] = long side dots;
			// context.size.v[1] = short side dots;
			// context.arguments => direction

			location = context.destination;
			position_ = context.size.v[1];
			numerator_ = position_ << 1;
			denominator_ = context.size.v[0] << 1;

			cycles = 32;
			access = AccessType::PlotPoint;
		}

		bool done() final {
			return !context.size.v[0];
		}

		void advance() final {
			--context.size.v[0];
			cycles = 88;

			// b0:	1 => long direction is y;
			//		0 => long direction is x.
			//
			// b2:	1 => x direction is left;
			//		0 => x direction is right.
			//
			// b3:	1 => y direction is up;
			//		0 => y direction is down.
			if(context.arguments & 0x1) {
				advance_axis<1>();
			} else {
				advance_axis<0>();
			}

			position_ -= numerator_;
			if(position_ < 0) {
				position_ += denominator_;
				cycles += 32;

				if(context.arguments & 0x1) {
					advance_axis<0>();
				} else {
					advance_axis<1>();
				}
			}
		}

	private:
		int position_, numerator_, denominator_, duration_;
};

/// Implements the PSET command, which plots a single pixel.
///
/// No timings are documented, so this'll output as quickly as possible.
struct PointSet: public Command {
	public:
		PointSet(CommandContext &context) : Command(context) {
			cycles = 0;
			access = AccessType::PlotPoint;
			location = context.destination;
		}

		bool done() final {
			return done_;
		}

		void advance() final {
			done_ = true;
		}

	private:
		bool done_ = false;
};

struct LogicalMoveFromCPU: public Command {
	public:
		LogicalMoveFromCPU(CommandContext &context) : Command(context) {
			is_cpu_transfer = true;

			start_x_ = context.destination.v[0];
			width_ = context.size.v[0];

			// This command is started with the first colour ready to transfer.
			cycles = 32;
			access = AccessType::PlotPoint;
			location = context.destination;
		}

		void advance() final {
			switch(access) {
				default: break;

				case AccessType::WaitForColourReceipt:
					cycles = 32;
					location = context.destination;
					access = AccessType::PlotPoint;
				break;

				case AccessType::PlotPoint:
					cycles = 0;
					access = AccessType::WaitForColourReceipt;
					advance_axis<0>();
					--context.size.v[0];

					if(!context.size.v[0]) {
						cycles = 64;
						context.size.v[0] = width_;
						context.destination.v[0] = start_x_;

						advance_axis<1>();
						--context.size.v[1];
					}
				break;
			}
		}

		bool done() final {
			return !context.size.v[1] || !width_;
		}

	private:
		int start_x_ = 0, width_ = 0;
};

struct HighSpeedFill: public Command {
	HighSpeedFill(CommandContext &context) : Command(context) {
		start_x_ = context.destination.v[0];
		width_ = context.size.v[0];

		cycles = 56;
		access = AccessType::WriteByte;
		location = context.destination;
	}

	bool done() final {
		return true;
	}

	void advance() final {
		cycles = 48;
	}

	private:
		int start_x_ = 0, width_ = 0;
};

}
}
}

#endif /* YamahaCommands_hpp */
