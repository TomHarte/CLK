//
//  YamahaCommands.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 26/01/2023.
//  Copyright © 2023 Thomas Harte. All rights reserved.
//

#pragma once

#include "AccessEnums.hpp"

namespace TI::TMS {

// MARK: - Generics.

struct Vector {
	int v[2]{};

	template <int offset, bool high> void set(const uint8_t value) {
		static constexpr uint8_t mask = high ? (offset ? 0x3 : 0x1) : 0xff;
		static constexpr int shift = high ? 8 : 0;
		v[offset] = (v[offset] & ~(mask << shift)) | ((value & mask) << shift);
	}

	template <int offset> void add(const int amount) {
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

struct Colour {
	void set(const uint8_t value) {
		colour = value;
		colour4bpp = uint8_t((value & 0xf) | (value << 4));
		colour2bpp = uint8_t((colour4bpp & 0x33) | ((colour4bpp & 0x33) << 2));
	}

	void reset() {
		colour = 0x00;
		colour4bpp = 0xff;
	}

	bool has_value() const {
		return (colour & 0xf) == (colour4bpp & 0xf);
	}

	/// Colour as written by the CPU.
	uint8_t colour = 0x00;
	/// The low four bits of the CPU-written colour, repeated twice.
	uint8_t colour4bpp = 0xff;
	/// The low two bits of the CPU-written colour, repeated four times.
	uint8_t colour2bpp = 0xff;
};

struct CommandContext {
	Vector source;
	Vector destination;
	Vector size;

	uint8_t arguments = 0;
	Colour colour;
	Colour latched_colour;

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

struct ModeDescription {
	int width = 256;
	int pixels_per_byte = 4;
	bool rotate_address = false;
	int start_cycle = 0;
	int end_cycle = 0;
};

struct Command {
	// In net:
	//
	// This command is blocked until @c access has been performed, reading
	// from or writing to @c value. It should not be performed until at least
	// @c cycles have passed.
	enum class AccessType {
		/// Plots a single pixel of the current contextual colour at @c destination,
		/// which occurs as a read, then a 24-cycle gap, then a write.
		PlotPoint,

		/// Blocks until the next CPU write to the colour register.
		WaitForColourReceipt,

		/// Writes an entire byte to the address containing the current @c destination.
		WriteByte,

		/// Copies a single pixel from @c source location to @c destination,
		/// being a read, a 32-cycle gap, then a PlotPoint.
		CopyPoint,

		/// Copies a complete byte from @c source location to @c destination,
		/// being a read, a 24-cycle gap, then a write.
		CopyByte,

		/// Copies a single pixel from @c source to the colour status register.
		ReadPoint,

//		ReadByte,
//		WaitForColourSend,
	};
	AccessType access = AccessType::PlotPoint;
	int cycles = 0;
	bool is_cpu_transfer = false;
	bool y_only = false;

	/// Current command parameters.
	CommandContext &context;
	ModeDescription &mode_description;
	Command(CommandContext &context, ModeDescription &mode_description) :
		context(context), mode_description(mode_description) {}
	virtual ~Command() = default;

	/// @returns @c true if all output from this command is done; @c false otherwise.
	virtual bool done() = 0;

	/// Repopulates the fields above with the next action to take, being provided with the
	/// number of pixels per byte in the current screen mode.
	virtual void advance() = 0;

	protected:
		template <int axis, bool include_source> void advance_axis(const int offset = 1) {
			context.destination.add<axis>(context.arguments & (0x4 << axis) ? -offset : offset);
			if constexpr (include_source) {
				context.source.add<axis>(context.arguments & (0x4 << axis) ? -offset : offset);
			}
		}
};

namespace Commands {

// MARK: - Line drawing.

/// Implements the LINE command, which is plain-old Bresenham.
///
/// Per Grauw timing is:
///
///	* 88 cycles between every pixel plot;
///	* plus an additional 32 cycles if a step along the minor axis is taken.
struct Line: public Command {
public:
	Line(CommandContext &context, ModeDescription &mode_description) : Command(context, mode_description) {
		// context.destination = start position;
		// context.size.v[0] = long side dots;
		// context.size.v[1] = short side dots;
		// context.arguments => direction

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
			advance_axis<1, false>();
		} else {
			advance_axis<0, false>();
		}

		position_ -= numerator_;
		if(position_ < 0) {
			position_ += denominator_;
			cycles += 32;

			if(context.arguments & 0x1) {
				advance_axis<0, false>();
			} else {
				advance_axis<1, false>();
			}
		}
	}

private:
	int position_, numerator_, denominator_;
};

// MARK: - Single pixel manipulation.

/// Implements the PSET command, which plots a single pixel and POINT, which reads one.
///
/// No timings are documented, so this'll output or input as quickly as possible.
template <bool is_read> struct Point: public Command {
public:
	Point(CommandContext &context, ModeDescription &mode_description) : Command(context, mode_description) {
		cycles = 0;	// TODO.
		access = is_read ? AccessType::ReadPoint : AccessType::PlotPoint;
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

// MARK: - Rectangular base.

/// Useful base class for anything that does logical work in a rectangle.
template <bool logical, bool include_source> struct Rectangle: public Command {
public:
	Rectangle(CommandContext &context, ModeDescription &mode_description) : Command(context, mode_description) {
		if constexpr (include_source) {
			start_x_[0] = context.source.v[0];
		}
		start_x_[1] = context.destination.v[0];
		width_ = context.size.v[0];

		if(!width_) {
			// Width = 0 => maximal width for this mode.
			// (aside: it's still unclear to me whether commands are
			// automatically clipped to the display; I think so but
			// don't want to spend any time on it until I'm certain)
//				context.size.v[0] = width_ = mode_description.width;
		}
	}

	/// Advances the current destination and, if @c include_source is @c true also the source;
	/// @returns @c true if a new row was started; @c false otherwise.
	bool advance_pixel() {
		if constexpr (logical) {
			advance_axis<0, include_source>();
			--context.size.v[0];

			if(context.size.v[0]) {
				return false;
			}
		} else {
			advance_axis<0, include_source>(mode_description.pixels_per_byte);
			context.size.v[0] -= mode_description.pixels_per_byte;

			if(context.size.v[0] & ~(mode_description.pixels_per_byte - 1)) {
				return false;
			}
		}

		context.size.v[0] = width_;
		if constexpr (include_source) {
			context.source.v[0] = start_x_[0];
		}
		context.destination.v[0] = start_x_[1];

		advance_axis<1, include_source>();
		--context.size.v[1];

		return true;
	}

	bool done() final {
		return !context.size.v[1] || !width_;
	}

private:
	int start_x_[2]{}, width_ = 0;
};

// MARK: - Rectangular moves to/from CPU.

template <bool logical> struct MoveFromCPU: public Rectangle<logical, false> {
	MoveFromCPU(CommandContext &context, ModeDescription &mode_description) :
		Rectangle<logical, false>(context, mode_description)
	{
		Command::is_cpu_transfer = true;

		// This command is started with the first colour ready to transfer.
		Command::cycles = 32;
		Command::access = logical ? Command::AccessType::PlotPoint : Command::AccessType::WriteByte;
	}

	void advance() final {
		switch(Command::access) {
			default: break;

			case Command::AccessType::WaitForColourReceipt:
				Command::cycles = 32;
				Command::access = logical ? Command::AccessType::PlotPoint : Command::AccessType::WriteByte;
			break;

			case Command::AccessType::WriteByte:
			case Command::AccessType::PlotPoint:
				Command::cycles = 0;
				Command::access = Command::AccessType::WaitForColourReceipt;
				if(Rectangle<logical, false>::advance_pixel()) {
					Command::cycles = 64;
					// TODO: I'm not sure this will be honoured per the outer wrapping.
				}
			break;
		}
	}
};

// MARK: - Rectangular moves within VRAM.

enum class MoveType {
	Logical,
	HighSpeed,
	YOnly,
};

template <MoveType type> struct Move: public Rectangle<type == MoveType::Logical, true> {
	static constexpr bool is_logical = type == MoveType::Logical;
	static constexpr bool is_y_only = type == MoveType::YOnly;
	using RectangleBase = Rectangle<is_logical, true>;

	Move(CommandContext &context, ModeDescription &mode_description) : RectangleBase(context, mode_description) {
		Command::access = is_logical ? Command::AccessType::CopyPoint : Command::AccessType::CopyByte;
		Command::cycles = is_y_only ? 0 : 64;
		Command::y_only = is_y_only;
	}

	void advance() final {
		Command::cycles = is_y_only ? 40 : 64;
		if(RectangleBase::advance_pixel()) {
			Command::cycles += is_y_only ? 0 : 64;
		}
	}
};

// MARK: - Rectangular fills.

template <bool logical> struct Fill: public Rectangle<logical, false> {
	using RectangleBase = Rectangle<logical, false>;

	Fill(CommandContext &context, ModeDescription &mode_description) : RectangleBase(context, mode_description) {
		Command::cycles = logical ? 64 : 56;
		Command::access = logical ? Command::AccessType::PlotPoint : Command::AccessType::WriteByte;
	}

	void advance() final {
		Command::cycles = logical ? 72 : 48;
		if(RectangleBase::advance_pixel()) {
			Command::cycles += logical ? 64 : 56;
		}
	}
};

}
}
