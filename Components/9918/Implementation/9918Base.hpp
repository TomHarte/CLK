//
//  9918Base.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 14/12/2017.
//  Copyright 2017 Thomas Harte. All rights reserved.
//

#ifndef TMS9918Base_hpp
#define TMS9918Base_hpp

#include "ClockConverter.hpp"

#include "../../../ClockReceiver/ClockReceiver.hpp"
#include "../../../Numeric/BitReverse.hpp"
#include "../../../Outputs/CRT/CRT.hpp"

#include "AccessEnums.hpp"
#include "LineBuffer.hpp"
#include "PersonalityTraits.hpp"
#include "Storage.hpp"
#include "YamahaCommands.hpp"

#include <array>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <memory>
#include <vector>

namespace TI {
namespace TMS {

constexpr uint8_t StatusInterrupt = 0x80;
constexpr uint8_t StatusSpriteOverflow = 0x40;

constexpr int StatusSpriteCollisionShift = 5;
constexpr uint8_t StatusSpriteCollision = 0x20;

template <Personality personality> struct Base: public Storage<personality> {
	Base();

	static constexpr int output_lag = 11;	// i.e. pixel output will occur 11 cycles
											// after corresponding data read.

	static constexpr uint32_t palette_pack(uint8_t r, uint8_t g, uint8_t b) {
		#if TARGET_RT_BIG_ENDIAN
			return uint32_t((r << 24) | (g << 16) | (b << 8));
		#else
			return uint32_t((b << 16) | (g << 8) | r);
		#endif
	}

	// The default TMS palette.
	static constexpr std::array<uint32_t, 16> default_palette {
		palette_pack(0, 0, 0),
		palette_pack(0, 0, 0),
		palette_pack(33, 200, 66),
		palette_pack(94, 220, 120),

		palette_pack(84, 85, 237),
		palette_pack(125, 118, 252),
		palette_pack(212, 82, 77),
		palette_pack(66, 235, 245),

		palette_pack(252, 85, 84),
		palette_pack(255, 121, 120),
		palette_pack(212, 193, 84),
		palette_pack(230, 206, 128),

		palette_pack(33, 176, 59),
		palette_pack(201, 91, 186),
		palette_pack(204, 204, 204),
		palette_pack(255, 255, 255)
	};
	const std::array<uint32_t, 16> &palette() {
		if constexpr (is_yamaha_vdp(personality)) {
			return Storage<personality>::palette_;
		}
		return default_palette;
	}

	Outputs::CRT::CRT crt_;
	TVStandard tv_standard_ = TVStandard::NTSC;
	using AddressT = typename Storage<personality>::AddressT;

	/// Mutates @c target such that @c source replaces the @c length bits that currently start
	/// at bit @c shift . Subsequently ensures @c target is constrained by the
	/// applicable @c memory_mask.
	template <int shift, int length = 8> void install_field(AddressT &target, uint8_t source) {
		static_assert(length > 0 && length <= 8);
		constexpr auto source_mask = (1 << length) - 1;
		constexpr auto mask = AddressT(~(source_mask << shift));
		target = (
			(target & mask) |
			AddressT((source & source_mask) << shift)
		) & memory_mask(personality);
	}

	// Personality-specific metrics and converters.
	ClockConverter<personality> clock_converter_;

	// This VDP's DRAM.
	std::array<uint8_t, memory_size(personality)> ram_;

	// State of the DRAM/CRAM-access mechanism.
	AddressT ram_pointer_ = 0;
	uint8_t read_ahead_buffer_ = 0;
	MemoryAccess queued_access_ = MemoryAccess::None;
	int minimum_access_column_ = 0;

	// The main status register.
	uint8_t status_ = 0;

	// Current state of programmer input.
	bool write_phase_ = false;	// Determines whether the VDP is expecting the low or high byte of a write.
	uint8_t low_write_ = 0;		// Buffers the low byte of a write.

	// Various programmable flags.
	bool mode1_enable_ = false;
	bool mode2_enable_ = false;
	bool mode3_enable_ = false;
	bool blank_display_ = false;
	bool sprites_16x16_ = false;
	bool sprites_magnified_ = false;
	bool generate_interrupts_ = false;
	int sprite_height_ = 8;

	// Programmer-specified addresses.
	//
	// The TMS and descendants combine various parts of the address with AND operations,
	// e.g. the fourth byte in the pattern name table will be at `pattern_name_address_ & 4`;
	// ordinarily the difference between that and plain substitution is invisible because
	// the programmer mostly can't set low-enough-order bits. That's not universally true
	// though, so this implementation uses AND throughout.
	//
	// ... therefore, all programmer-specified addresses are seeded as all '1's. As and when
	// actual addresses are specified, the relevant bits will be substituted in.
	//
	// Cf. install_field.
	AddressT pattern_name_address_ = memory_mask(personality);				// Address of the tile map.
	AddressT colour_table_address_ = memory_mask(personality);				// Address of the colour map (if applicable).
	AddressT pattern_generator_table_address_ = memory_mask(personality);	// Address of the tile contents.
	AddressT sprite_attribute_table_address_ = memory_mask(personality);	// Address of the sprite list.
	AddressT sprite_generator_table_address_ = memory_mask(personality);	// Address of the sprite contents.

	// Default colours.
	uint8_t text_colour_ = 0;
	uint8_t background_colour_ = 0;

	// Internal mechanisms for position tracking.
	int latched_column_ = 0;

	// A struct to contain timing information that is a function of the current mode.
	struct {
		/*
			Vertical layout:

			Lines 0 to [pixel_lines]: standard data fetch and drawing will occur.
			... to [first_vsync_line]: refresh fetches will occur and border will be output.
			.. to [2.5 or 3 lines later]: vertical sync is output.
			... to [total lines - 1]: refresh fetches will occur and border will be output.
			... for one line: standard data fetch will occur, without drawing.
		*/
		int total_lines = 262;
		int pixel_lines = 192;
		int first_vsync_line = 227;

		// Maximum number of sprite slots to populate;
		// if sprites beyond this number should be visible
		// then the appropriate status information will be set.
		int maximum_visible_sprites = 4;

		// Set the position, in cycles, of the two interrupts,
		// within a line.
		struct {
			int column = 4;
			int row = 193;
		} end_of_frame_interrupt_position;
		int line_interrupt_position = -1;

		// Enables or disabled the recognition of the sprite
		// list terminator, and sets the terminator value.
		bool allow_sprite_terminator = true;
		uint8_t sprite_terminator = 0xd0;
	} mode_timing_;

	uint8_t line_interrupt_target_ = 0xff;
	uint8_t line_interrupt_counter_ = 0;
	bool enable_line_interrupts_ = false;
	bool line_interrupt_pending_ = false;

	ScreenMode screen_mode_, underlying_mode_;
	LineBuffer line_buffers_[313];
	AddressT tile_offset_ = 0;
	uint8_t name_[4]{};
	void posit_sprite(LineBuffer &buffer, int sprite_number, int sprite_y, int screen_row);

	// There is a delay between reading into the line buffer and outputting from there to the screen. That delay
	// is observeable because reading time affects availability of memory accesses and therefore time in which
	// to update sprites and tiles, but writing time affects when the palette is used and when the collision flag
	// may end up being set. So the two processes are slightly decoupled. The end of reading one line may overlap
	// with the beginning of writing the next, hence the two separate line buffers.
	LineBufferPointer output_pointer_, fetch_pointer_;

	int fetch_line() const;
	bool is_horizontal_blank() const;
	VerticalState vertical_state() const;

	int masked_address(int address) const;
	void write_vram(uint8_t);
	void write_register(uint8_t);
	void write_palette(uint8_t);
	void write_register_indirect(uint8_t);
	uint8_t read_vram();
	uint8_t read_register();
	uint8_t read_palette();
	uint8_t read_register_indirect();

	void commit_register(int reg, uint8_t value);

	template <bool check_blank> ScreenMode current_screen_mode() const {
		if(check_blank && blank_display_) {
			return ScreenMode::Blank;
		}

		if constexpr (is_sega_vdp(personality)) {
			if(Storage<personality>::mode4_enable_) {
				return ScreenMode::SMSMode4;
			}
		}

		if constexpr (is_yamaha_vdp(personality)) {
			switch(Storage<personality>::mode_) {
				case 0b00001:	return ScreenMode::Text;
				case 0b01001:	return ScreenMode::YamahaText80;
				case 0b00010:	return ScreenMode::MultiColour;
				case 0b00000:	return ScreenMode::YamahaGraphics1;
				case 0b00100:	return ScreenMode::YamahaGraphics2;
				case 0b01000:	return ScreenMode::YamahaGraphics3;
				case 0b01100:	return ScreenMode::YamahaGraphics4;
				case 0b10000:	return ScreenMode::YamahaGraphics5;
				case 0b10100:	return ScreenMode::YamahaGraphics6;
				case 0b11100:	return ScreenMode::YamahaGraphics7;
			}
		}

		if(!mode1_enable_ && !mode2_enable_ && !mode3_enable_) {
			return ScreenMode::ColouredText;
		}

		if(mode1_enable_ && !mode2_enable_ && !mode3_enable_) {
			return ScreenMode::Text;
		}

		if(!mode1_enable_ && mode2_enable_ && !mode3_enable_) {
			return ScreenMode::Graphics;
		}

		if(!mode1_enable_ && !mode2_enable_ && mode3_enable_) {
			return ScreenMode::MultiColour;
		}

		// TODO: undocumented TMS modes.
		return ScreenMode::Blank;
	}

	AddressT command_address(Vector location) const {
		if constexpr (is_yamaha_vdp(personality)) {
			switch(this->screen_mode_) {
				default:
				case ScreenMode::YamahaGraphics4:	// 256 pixels @ 4bpp
					return AddressT(
						(location.v[0] >> 1) +
						(location.v[1] << 7)
					);

				case ScreenMode::YamahaGraphics5:	// 512 pixels @ 2bpp
					return AddressT(
						(location.v[0] >> 2) +
						(location.v[1] << 7)
					);

				case ScreenMode::YamahaGraphics6:	// 512 pixels @ 4bpp
					return AddressT(
						(location.v[0] >> 1) +
						(location.v[1] << 8)
					);

				case ScreenMode::YamahaGraphics7:	// 256 pixels @ 8bpp
					return AddressT(
						(location.v[0] >> 0) +
						(location.v[1] << 8)
					);
			}
		} else {
			return 0;
		}
	}

	uint8_t extract_colour(uint8_t byte, Vector location) const {
		switch(this->screen_mode_) {
			default:
			case ScreenMode::YamahaGraphics4:	// 256 pixels @ 4bpp
			case ScreenMode::YamahaGraphics6:	// 512 pixels @ 4bpp
				return (byte >> (((location.v[0] & 1) ^ 1) << 2)) & 0xf;

			case ScreenMode::YamahaGraphics5:	// 512 pixels @ 2bpp
				return (byte >> (((location.v[0] & 3) ^ 3) << 1)) & 0x3;

			case ScreenMode::YamahaGraphics7:	// 256 pixels @ 8bpp
				return byte;
		}
	}

	std::pair<uint8_t, uint8_t> command_colour_mask(Vector location) const {
		if constexpr (is_yamaha_vdp(personality)) {
			auto &context = Storage<personality>::command_context_;
			auto colour = context.latched_colour.has_value() ? context.latched_colour : context.colour;

			switch(this->screen_mode_) {
				default:
				case ScreenMode::YamahaGraphics4:	// 256 pixels @ 4bpp
				case ScreenMode::YamahaGraphics6:	// 512 pixels @ 4bpp
					return
						std::make_pair(
							0xf0 >> ((location.v[0] & 1) << 2),
							colour.colour4bpp
						);

				case ScreenMode::YamahaGraphics5:	// 512 pixels @ 2bpp
					return
						std::make_pair(
							0xc0 >> ((location.v[0] & 3) << 1),
							colour.colour2bpp
						);

				case ScreenMode::YamahaGraphics7:	// 256 pixels @ 8bpp
					return
						std::make_pair(
							0xff,
							colour.colour
						);
			}
		} else {
			return std::make_pair(0, 0);
		}
	}

	void do_external_slot(int access_column) {
		// Don't do anything if the required time for the access to become executable
		// has yet to pass.
		if(queued_access_ == MemoryAccess::None || access_column < minimum_access_column_) {
			if constexpr (is_yamaha_vdp(personality)) {
				using CommandStep = typename Storage<personality>::CommandStep;

				if(
					Storage<personality>::next_command_step_ == CommandStep::None ||
					access_column < Storage<personality>::minimum_command_column_
				) {
					return;
				}

				auto &context = Storage<personality>::command_context_;
				switch(Storage<personality>::next_command_step_) {
					// Duplicative, but keeps the compiler happy.
					case CommandStep::None:
					break;

					case CommandStep::ReadSourcePixel:
						context.latched_colour.set(extract_colour(ram_[command_address(context.source)], context.source));

						Storage<personality>::minimum_command_column_ = access_column + 32;
						Storage<personality>::next_command_step_ = CommandStep::ReadDestinationPixel;
					break;

					case CommandStep::ReadDestinationPixel:
						Storage<personality>::command_latch_ = ram_[command_address(context.destination)];

						Storage<personality>::minimum_command_column_ = access_column + 24;
						Storage<personality>::next_command_step_ = CommandStep::WritePixel;
					break;

					case CommandStep::WritePixel: {
						const auto [mask, unmasked_colour] = command_colour_mask(context.destination);
						const auto address = command_address(context.destination);
						const uint8_t colour = unmasked_colour & mask;
						context.latched_colour.reset();

						using LogicalOperation = CommandContext::LogicalOperation;
						if(!context.test_source || colour) {
							switch(context.pixel_operation) {
								default:
								case LogicalOperation::Copy:
									Storage<personality>::command_latch_ &= ~mask;
									Storage<personality>::command_latch_ |= colour;
								break;
								case LogicalOperation::And:
									Storage<personality>::command_latch_ &= ~mask | colour;
								break;
								case LogicalOperation::Or:
									Storage<personality>::command_latch_ |= colour;
								break;
								case LogicalOperation::Xor:
									Storage<personality>::command_latch_ ^= colour;
								break;
								case LogicalOperation::Not:
									Storage<personality>::command_latch_ &= ~mask;
									Storage<personality>::command_latch_ |= colour ^ mask;
								break;
							}
						}

						ram_[address] = Storage<personality>::command_latch_;

						Storage<personality>::command_->advance(pixels_per_byte(this->underlying_mode_));
						Storage<personality>::update_command_step(access_column);
					} break;

					case CommandStep::ReadSourceByte:
						context.latched_colour.set(ram_[command_address(context.source)]);

						Storage<personality>::minimum_command_column_ = access_column + 24;
						Storage<personality>::next_command_step_ = CommandStep::WriteByte;
					break;

					case CommandStep::WriteByte:
						ram_[command_address(context.destination)] = context.latched_colour.has_value() ? context.latched_colour.colour : context.colour.colour;
						context.latched_colour.reset();

						Storage<personality>::command_->advance(pixels_per_byte(this->underlying_mode_));
						Storage<personality>::update_command_step(access_column);
					break;
				}
			}

			return;
		}

		AddressT address = ram_pointer_;
		++ram_pointer_;

		if constexpr (is_yamaha_vdp(personality)) {
			// The Yamaha increments only 14 bits of the address in TMS-compatible modes.
			if(this->underlying_mode_ < ScreenMode::YamahaText80) {
				ram_pointer_ = (ram_pointer_ & 0x3fff) | (address & AddressT(~0x3fff));
			}

			if(this->underlying_mode_ == ScreenMode::YamahaGraphics6 || this->underlying_mode_ == ScreenMode::YamahaGraphics7) {
				// Rotate address one to the right as the hardware accesses
				// the underlying banks of memory alternately but presents
				// them as if linear.
				address = (address >> 1) | (address << 16);
			}
		}

		switch(queued_access_) {
			default: break;

			case MemoryAccess::Write:
				if constexpr (is_sega_vdp(personality)) {
					if(Storage<personality>::cram_is_selected_) {
						// Adjust the palette. In a Master System blue has a slightly different
						// scale; cf. https://www.retrorgb.com/sega-master-system-non-linear-blue-channel-findings.html
						constexpr uint8_t rg_scale[] = {0, 85, 170, 255};
						constexpr uint8_t b_scale[] = {0, 104, 170, 255};
						Storage<personality>::colour_ram_[address & 0x1f] = palette_pack(
							rg_scale[(read_ahead_buffer_ >> 0) & 3],
							rg_scale[(read_ahead_buffer_ >> 2) & 3],
							b_scale[(read_ahead_buffer_ >> 4) & 3]
						);

						// Schedule a CRAM dot; this is scheduled for wherever it should appear
						// on screen. So it's wherever the output stream would be now. Which
						// is output_lag cycles ago from the point of view of the input stream.
						auto &dot = Storage<personality>::upcoming_cram_dots_.emplace_back();
						dot.location.column = fetch_pointer_.column - output_lag;
						dot.location.row = fetch_pointer_.row;

						// Handle before this row conditionally; then handle after (or, more realistically,
						// exactly at the end of) naturally.
						if(dot.location.column < 0) {
							--dot.location.row;
							dot.location.column += 342;
						}
						dot.location.row += dot.location.column / 342;
						dot.location.column %= 342;

						dot.value = Storage<personality>::colour_ram_[address & 0x1f];
						break;
					}
				}
				ram_[address & memory_mask(personality)] = read_ahead_buffer_;
			break;
			case MemoryAccess::Read:
				read_ahead_buffer_ = ram_[address & memory_mask(personality)];
			break;
		}
		queued_access_ = MemoryAccess::None;
	}

	/// Helper for TMS dispatches; contains a switch statement with cases 0 to 170, each of the form:
	///
	/// 	if constexpr (use_end && end == n) return; [[fallthrough]]; case n: fetcher.fetch<n>();
	///
	/// i.e. it provides standard glue to enter a fetch sequence at any point, while the fetches themselves are templated on the cycle
	/// at which they appear for neater expression.
	template<bool use_end, typename Fetcher> void dispatch(Fetcher &fetcher, int start, int end);

	// Various fetchers.
	template<bool use_end> void fetch_tms_refresh(LineBuffer &, LineBuffer &, int y, int start, int end);
	template<bool use_end> void fetch_tms_text(LineBuffer &, LineBuffer &, int y, int start, int end);
	template<bool use_end> void fetch_tms_character(LineBuffer &, LineBuffer &, int y, int start, int end);

	template<bool use_end> void fetch_yamaha(LineBuffer &, LineBuffer &, int y, int start, int end);
	template<ScreenMode> void fetch_yamaha(LineBuffer &, LineBuffer &, int y, int end);

	template<bool use_end> void fetch_sms(LineBuffer &, LineBuffer &, int y, int start, int end);

	// A helper function to output the current border colour for
	// the number of cycles supplied.
	void output_border(int cycles, uint32_t cram_dot);

	// Output serialisation state.
	uint32_t *pixel_target_ = nullptr, *pixel_origin_ = nullptr;
	bool asked_for_write_area_ = false;

	// Output serialisers.
	void draw_tms_character(int start, int end);
	template <bool apply_blink> void draw_tms_text(int start, int end);
	void draw_sms(int start, int end, uint32_t cram_dot);

	template<ScreenMode mode> void draw_yamaha(LineBuffer &, int start, int end);
	void draw_yamaha(int start, int end);
};

#include "Fetch.hpp"
#include "Draw.hpp"

}
}

#endif /* TMS9918Base_hpp */
