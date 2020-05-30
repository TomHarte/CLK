//
//  9918Base.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 14/12/2017.
//  Copyright 2017 Thomas Harte. All rights reserved.
//

#ifndef TMS9918Base_hpp
#define TMS9918Base_hpp

#include "../../../Outputs/CRT/CRT.hpp"
#include "../../../ClockReceiver/ClockReceiver.hpp"

#include <cassert>
#include <cstdint>
#include <memory>
#include <vector>

namespace TI {
namespace TMS {

enum Personality {
	TMS9918A,	// includes the 9928 and 9929; set TV standard and output device as desired.
	V9938,
	V9958,
	SMSVDP,
	SMS2VDP,
	GGVDP,
};

enum class TVStandard {
	/*! i.e. 50Hz output at around 312.5 lines/field */
	PAL,
	/*! i.e. 60Hz output at around 262.5 lines/field */
	NTSC
};

#define is_sega_vdp(x) ((x) >= SMSVDP)

class Base {
	public:
		static uint32_t palette_pack(uint8_t r, uint8_t g, uint8_t b) {
			uint32_t result = 0;
			uint8_t *const result_ptr = reinterpret_cast<uint8_t *>(&result);
			result_ptr[0] = r;
			result_ptr[1] = g;
			result_ptr[2] = b;
			result_ptr[3] = 0;
			return result;
		}

	protected:
		static constexpr int output_lag = 11;	// i.e. pixel output will occur 11 cycles after corresponding data read.

		// The default TMS palette.
		const uint32_t palette[16] = {
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

		Base(Personality p);

		const Personality personality_;
		Outputs::CRT::CRT crt_;
		TVStandard tv_standard_ = TVStandard::NTSC;

		// Holds the contents of this VDP's connected DRAM.
		std::vector<uint8_t> ram_;

		// Holds the state of the DRAM/CRAM-access mechanism.
		uint16_t ram_pointer_ = 0;
		uint8_t read_ahead_buffer_ = 0;
		enum class MemoryAccess {
			Read, Write, None
		} queued_access_ = MemoryAccess::None;
		int cycles_until_access_ = 0;
		int minimum_access_column_ = 0;
		int vram_access_delay() {
			// This seems to be correct for all currently-modelled VDPs;
			// it's the delay between an external device scheduling a
			// read or write and the very first time that can occur
			// (though, in practice, it won't happen until the next
			// external slot after this number of cycles after the
			// device has requested the read or write).
			return 6;
		}

		// Holds the main status register.
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

		size_t pattern_name_address_ = 0;				// i.e. address of the tile map.
		size_t colour_table_address_ = 0;				// address of the colour map (if applicable).
		size_t pattern_generator_table_address_ = 0;	// address of the tile contents.
		size_t sprite_attribute_table_address_ = 0;		// address of the sprite list.
		size_t sprite_generator_table_address_ = 0;		// address of the sprite contents.

		uint8_t text_colour_ = 0;
		uint8_t background_colour_ = 0;

		// This implementation of this chip officially accepts a 3.58Mhz clock, but runs
		// internally at 5.37Mhz. The following two help to maintain a lossless conversion
		// from the one to the other.
		int cycles_error_ = 0;
		HalfCycles half_cycles_before_internal_cycles(int internal_cycles);

		// Internal mechanisms for position tracking.
		int latched_column_ = 0;

		// A helper function to output the current border colour for
		// the number of cycles supplied.
		void output_border(int cycles, uint32_t cram_dot);

		// A struct to contain timing information for the current mode.
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

		uint8_t line_interrupt_target = 0xff;
		uint8_t line_interrupt_counter = 0;
		bool enable_line_interrupts_ = false;
		bool line_interrupt_pending_ = false;

		// The screen mode is a necessary predecessor to picking the line mode,
		// which is the thing latched per line.
		enum class ScreenMode {
			Blank,
			Text,
			MultiColour,
			ColouredText,
			Graphics,
			SMSMode4
		} screen_mode_;

		enum class LineMode {
			Text,
			Character,
			Refresh,
			SMS
		};

		// Temporary buffers collect a representation of this line prior to pixel serialisation.
		struct LineBuffer {
			// The line mode describes the proper timing diagram for this line.
			LineMode line_mode = LineMode::Text;

			// Holds the horizontal scroll position to apply to this line;
			// of those VDPs currently implemented, affects the Master System only.
			uint8_t latched_horizontal_scroll = 0;

			// The names array holds pattern names, as an offset into memory, and
			// potentially flags also.
			struct {
				size_t offset = 0;
				uint8_t flags = 0;
			} names[40];

			// The patterns array holds tile patterns, corresponding 1:1 with names.
			// Four bytes per pattern is the maximum required by any
			// currently-implemented VDP.
			uint8_t patterns[40][4];

			/*
				Horizontal layout (on a 342-cycle clock):

					15 cycles right border
					58 cycles blanking & sync
					13 cycles left border

					... i.e. to cycle 86, then:

					border up to first_pixel_output_column;
					pixels up to next_border_column;
					border up to the end.

				e.g. standard 256-pixel modes will want to set
				first_pixel_output_column = 86, next_border_column = 342.
			*/
			int first_pixel_output_column = 94;
			int next_border_column = 334;

			// An active sprite is one that has been selected for composition onto
			// this line.
			struct ActiveSprite {
				int index = 0;		// The original in-table index of this sprite.
				int row = 0;		// The row of the sprite that should be drawn.
				int x = 0;			// The sprite's x position on screen.

				uint8_t image[4];		// Up to four bytes of image information.
				int shift_position = 0;	// An offset representing how much of the image information has already been drawn.
			} active_sprites[8];

			int active_sprite_slot = 0;		// A pointer to the slot into which a new active sprite will be deposited, if required.
			bool sprites_stopped = false;	// A special TMS feature is that a sentinel value can be used to prevent any further sprites
											// being evaluated for display. This flag determines whether the sentinel has yet been reached.

			void reset_sprite_collection();
		} line_buffers_[313];
		void posit_sprite(LineBuffer &buffer, int sprite_number, int sprite_y, int screen_row);

		// There is a delay between reading into the line buffer and outputting from there to the screen. That delay
		// is observeable because reading time affects availability of memory accesses and therefore time in which
		// to update sprites and tiles, but writing time affects when the palette is used and when the collision flag
		// may end up being set. So the two processes are slightly decoupled. The end of reading one line may overlap
		// with the beginning of writing the next, hence the two separate line buffers.
		struct LineBufferPointer {
			int row, column;
		} read_pointer_, write_pointer_;

		// The SMS VDP has a programmer-set colour palette, with a dedicated patch of RAM. But the RAM is only exactly
		// fast enough for the pixel clock. So when the programmer writes to it, that causes a one-pixel glitch; there
		// isn't the bandwidth for the read both write to occur simultaneously. The following buffer therefore keeps
		// track of pending collisions, for visual reproduction.
		struct CRAMDot {
			LineBufferPointer location;
			uint32_t value;
		};
		std::vector<CRAMDot> upcoming_cram_dots_;

		// Extra information that affects the Master System output mode.
		struct {
			// Programmer-set flags.
			bool vertical_scroll_lock = false;
			bool horizontal_scroll_lock = false;
			bool hide_left_column = false;
			bool shift_sprites_8px_left = false;
			bool mode4_enable = false;
			uint8_t horizontal_scroll = 0;
			uint8_t vertical_scroll = 0;

			// The Master System's additional colour RAM.
			uint32_t colour_ram[32];
			bool cram_is_selected = false;

			// Holds the vertical scroll position for this frame; this is latched
			// once and cannot dynamically be changed until the next frame.
			uint8_t latched_vertical_scroll = 0;

			size_t pattern_name_address;
			size_t sprite_attribute_table_address;
			size_t sprite_generator_table_address;
		} master_system_;

		void set_current_screen_mode() {
			if(blank_display_) {
				screen_mode_ = ScreenMode::Blank;
				return;
			}

			if(is_sega_vdp(personality_) && master_system_.mode4_enable) {
				screen_mode_ = ScreenMode::SMSMode4;
				mode_timing_.maximum_visible_sprites = 8;
				return;
			}

			mode_timing_.maximum_visible_sprites = 4;
			if(!mode1_enable_ && !mode2_enable_ && !mode3_enable_) {
				screen_mode_ = ScreenMode::ColouredText;
				return;
			}

			if(mode1_enable_ && !mode2_enable_ && !mode3_enable_) {
				screen_mode_ = ScreenMode::Text;
				return;
			}

			if(!mode1_enable_ && mode2_enable_ && !mode3_enable_) {
				screen_mode_ = ScreenMode::Graphics;
				return;
			}

			if(!mode1_enable_ && !mode2_enable_ && mode3_enable_) {
				screen_mode_ = ScreenMode::MultiColour;
				return;
			}

			// TODO: undocumented TMS modes.
			screen_mode_ = ScreenMode::Blank;
		}

		void do_external_slot(int access_column) {
			// Don't do anything if the required time for the access to become executable
			// has yet to pass.
			if(access_column < minimum_access_column_) {
				return;
			}

			switch(queued_access_) {
				default: return;

				case MemoryAccess::Write:
					if(master_system_.cram_is_selected) {
						// Adjust the palette.
						master_system_.colour_ram[ram_pointer_ & 0x1f] = palette_pack(
							uint8_t(((read_ahead_buffer_ >> 0) & 3) * 255 / 3),
							uint8_t(((read_ahead_buffer_ >> 2) & 3) * 255 / 3),
							uint8_t(((read_ahead_buffer_ >> 4) & 3) * 255 / 3)
						);

						// Schedule a CRAM dot; this is scheduled for wherever it should appear
						// on screen. So it's wherever the output stream would be now. Which
						// is output_lag cycles ago from the point of view of the input stream.
						upcoming_cram_dots_.emplace_back();
						CRAMDot &dot = upcoming_cram_dots_.back();

						dot.location.column = write_pointer_.column - output_lag;
						dot.location.row = write_pointer_.row;

						// Handle before this row conditionally; then handle after (or, more realistically,
						// exactly at the end of) naturally.
						if(dot.location.column < 0) {
							--dot.location.row;
							dot.location.column += 342;
						}
						dot.location.row += dot.location.column / 342;
						dot.location.column %= 342;

						dot.value = master_system_.colour_ram[ram_pointer_ & 0x1f];
					} else {
						ram_[ram_pointer_ & 16383] = read_ahead_buffer_;
					}
				break;
				case MemoryAccess::Read:
					read_ahead_buffer_ = ram_[ram_pointer_ & 16383];
				break;
			}
			++ram_pointer_;
			queued_access_ = MemoryAccess::None;
		}

/*
	Fetching routines follow below; they obey the following rules:

		1)	input is a start position and an end position; they should perform the proper
			operations for the period: start <= time < end.
		2)	times are measured relative to a 172-cycles-per-line clock (so: they directly
			count access windows on the TMS and Master System).
		3)	time 0 is the beginning of the access window immediately after the last pattern/data
			block fetch that would contribute to this line, in a normal 32-column mode. So:

				* it's cycle 309 on Mattias' TMS diagram;
				* it's cycle 1238 on his V9938 diagram;
				* it's after the last background render block in Mask of Destiny's Master System timing diagram.

			That division point was selected, albeit arbitrarily, because it puts all the tile
			fetches for a single line into the same [0, 171] period.

		4)	all of these functions are templated with a `use_end` parameter. That will be true if
			end is < 172, false otherwise. So functions can use it to eliminate should-exit-not checks,
			for the more usual path of execution.

	Provided for the benefit of the methods below:

		*	the function external_slot(), which will perform any pending VRAM read/write.
		*	the macros slot(n) and external_slot(n) which can be used to schedule those things inside a
			switch(start)-based implementation.

	All functions should just spool data to intermediary storage. This is because for most VDPs there is
	a decoupling between fetch pattern and output pattern, and it's neater to keep the same division
	for the exceptions.
*/

#define slot(n)	\
		if(use_end && end == n) return;\
		case n

#define external_slot(n)	\
	slot(n): do_external_slot((n)*2);

#define external_slots_2(n)	\
	external_slot(n);		\
	external_slot(n+1);

#define external_slots_4(n)	\
	external_slots_2(n);	\
	external_slots_2(n+2);

#define external_slots_8(n)	\
	external_slots_4(n);	\
	external_slots_4(n+4);

#define external_slots_16(n)	\
	external_slots_8(n);		\
	external_slots_8(n+8);

#define external_slots_32(n)	\
	external_slots_16(n);		\
	external_slots_16(n+16);


/***********************************************
	TMS9918 Fetching Code
************************************************/

		template<bool use_end> void fetch_tms_refresh(int start, int end) {
#define refresh(location)		\
	slot(location):				\
	external_slot(location+1);

#define refreshes_2(location)	\
	refresh(location);			\
	refresh(location+2);

#define refreshes_4(location)	\
	refreshes_2(location);		\
	refreshes_2(location+4);

#define refreshes_8(location)	\
	refreshes_4(location);		\
	refreshes_4(location+8);

			switch(start) {
				default: assert(false);

				/* 44 external slots */
				external_slots_32(0)
				external_slots_8(32)
				external_slots_4(40)

				/* 64 refresh/external slot pairs (= 128 windows) */
				refreshes_8(44);
				refreshes_8(60);
				refreshes_8(76);
				refreshes_8(92);
				refreshes_8(108);
				refreshes_8(124);
				refreshes_8(140);
				refreshes_8(156);

				return;
			}

#undef refreshes_8
#undef refreshes_4
#undef refreshes_2
#undef refresh
		}

		template<bool use_end> void fetch_tms_text(int start, int end) {
#define fetch_tile_name(location, column)		slot(location): line_buffer.names[column].offset = ram_[row_base + column];
#define fetch_tile_pattern(location, column)	slot(location): line_buffer.patterns[column][0] = ram_[row_offset + size_t(line_buffer.names[column].offset << 3)];

#define fetch_column(location, column)	\
	fetch_tile_name(location, column);	\
	external_slot(location+1);	\
	fetch_tile_pattern(location+2, column);

#define fetch_columns_2(location, column)	\
	fetch_column(location, column);	\
	fetch_column(location+3, column+1);

#define fetch_columns_4(location, column)	\
	fetch_columns_2(location, column);	\
	fetch_columns_2(location+6, column+2);

#define fetch_columns_8(location, column)	\
	fetch_columns_4(location, column);	\
	fetch_columns_4(location+12, column+4);

			LineBuffer &line_buffer = line_buffers_[write_pointer_.row];
			const size_t row_base = pattern_name_address_ & (0x3c00 | size_t(write_pointer_.row >> 3) * 40);
			const size_t row_offset = pattern_generator_table_address_ & (0x3800 | (write_pointer_.row & 7));

			switch(start) {
				default: assert(false);

					/* 47 external slots (= 47 windows) */
					external_slots_32(0)
					external_slots_8(32)
					external_slots_4(40)
					external_slots_2(44)
					external_slot(46)

					/* 40 column fetches (= 120 windows) */
					fetch_columns_8(47, 0);
					fetch_columns_8(71, 8);
					fetch_columns_8(95, 16);
					fetch_columns_8(119, 24);
					fetch_columns_8(143, 32);

					/* 5 more external slots */
					external_slots_4(167);
					external_slot(171);

				return;
			}

#undef fetch_columns_8
#undef fetch_columns_4
#undef fetch_columns_2
#undef fetch_column
#undef fetch_tile_pattern
#undef fetch_tile_name
		}

		template<bool use_end> void fetch_tms_character(int start, int end) {
#define sprite_fetch_coordinates(location, sprite)	\
	slot(location):		\
	slot(location+1):	\
		line_buffer.active_sprites[sprite].x = \
			ram_[\
				sprite_attribute_table_address_ & size_t(0x3f81 | (line_buffer.active_sprites[sprite].index << 2))\
			];

		// This implementation doesn't refetch Y; it's unclear to me
		// whether it's refetched.

#define sprite_fetch_graphics(location, sprite)	\
	slot(location):		\
	slot(location+1):	\
	slot(location+2):	\
	slot(location+3):	{\
		const uint8_t name = ram_[\
				sprite_attribute_table_address_ & size_t(0x3f82 | (line_buffer.active_sprites[sprite].index << 2))\
			] & (sprites_16x16_ ? ~3 : ~0);\
		line_buffer.active_sprites[sprite].image[2] = ram_[\
				sprite_attribute_table_address_ & size_t(0x3f83 | (line_buffer.active_sprites[sprite].index << 2))\
			];\
		line_buffer.active_sprites[sprite].x -= (line_buffer.active_sprites[sprite].image[2] & 0x80) >> 2;\
		const size_t graphic_location = sprite_generator_table_address_ & size_t(0x3800 | (name << 3) | line_buffer.active_sprites[sprite].row);	\
		line_buffer.active_sprites[sprite].image[0] = ram_[graphic_location];\
		line_buffer.active_sprites[sprite].image[1] = ram_[graphic_location+16];\
	}

#define sprite_fetch_block(location, sprite)	\
	sprite_fetch_coordinates(location, sprite)	\
	sprite_fetch_graphics(location+2, sprite)

#define sprite_y_read(location, sprite)	\
	slot(location): posit_sprite(sprite_selection_buffer, sprite, ram_[sprite_attribute_table_address_ & (((sprite) << 2) | 0x3f80)], write_pointer_.row);

#define fetch_tile_name(column) line_buffer.names[column].offset = ram_[(row_base + column) & 0x3fff];

#define fetch_tile(column)	{\
		line_buffer.patterns[column][1] = ram_[(colour_base + size_t((line_buffer.names[column].offset << 3) >> colour_name_shift)) & 0x3fff];		\
		line_buffer.patterns[column][0] = ram_[(pattern_base + size_t(line_buffer.names[column].offset << 3)) & 0x3fff];	\
	}

#define background_fetch_block(location, column, sprite)	\
	slot(location):	fetch_tile_name(column)		\
	external_slot(location+1);	\
	slot(location+2):	\
	slot(location+3): fetch_tile(column)	\
	slot(location+4): fetch_tile_name(column+1)	\
	sprite_y_read(location+5, sprite);	\
	slot(location+6):	\
	slot(location+7): fetch_tile(column+1)	\
	slot(location+8): fetch_tile_name(column+2)	\
	sprite_y_read(location+9, sprite+1);	\
	slot(location+10):	\
	slot(location+11): fetch_tile(column+2)	\
	slot(location+12): fetch_tile_name(column+3)	\
	sprite_y_read(location+13, sprite+2);	\
	slot(location+14):	\
	slot(location+15): fetch_tile(column+3)

			LineBuffer &line_buffer = line_buffers_[write_pointer_.row];
			LineBuffer &sprite_selection_buffer = line_buffers_[(write_pointer_.row + 1) % mode_timing_.total_lines];
			const size_t row_base = pattern_name_address_ & (size_t((write_pointer_.row << 2)&~31) | 0x3c00);

			size_t pattern_base = pattern_generator_table_address_;
			size_t colour_base = colour_table_address_;
			int colour_name_shift = 6;

			if(screen_mode_ == ScreenMode::Graphics) {
				// If this is high resolution mode, allow the row number to affect the pattern and colour addresses.
				pattern_base &= size_t(0x2000 | ((write_pointer_.row & 0xc0) << 5));
				colour_base &= size_t(0x2000 | ((write_pointer_.row & 0xc0) << 5));

				colour_base += size_t(write_pointer_.row & 7);
				colour_name_shift = 0;
			} else {
				colour_base &= size_t(0xffc0);
				pattern_base &= size_t(0x3800);
			}

			if(screen_mode_ == ScreenMode::MultiColour) {
				pattern_base += size_t((write_pointer_.row >> 2) & 7);
			} else {
				pattern_base += size_t(write_pointer_.row & 7);
			}

			switch(start) {
				default: assert(false);

				external_slots_2(0);

				sprite_fetch_block(2, 0);
				sprite_fetch_block(8, 1);
				sprite_fetch_coordinates(14, 2);

				external_slots_4(16);
				external_slot(20);

				sprite_fetch_graphics(21, 2);
				sprite_fetch_block(25, 3);

				slot(31):
					sprite_selection_buffer.reset_sprite_collection();
					do_external_slot(31*2);
				external_slots_2(32);
				external_slot(34);

				sprite_y_read(35, 0);
				sprite_y_read(36, 1);
				sprite_y_read(37, 2);
				sprite_y_read(38, 3);
				sprite_y_read(39, 4);
				sprite_y_read(40, 5);
				sprite_y_read(41, 6);
				sprite_y_read(42, 7);

				background_fetch_block(43, 0, 8);
				background_fetch_block(59, 4, 11);
				background_fetch_block(75, 8, 14);
				background_fetch_block(91, 12, 17);
				background_fetch_block(107, 16, 20);
				background_fetch_block(123, 20, 23);
				background_fetch_block(139, 24, 26);
				background_fetch_block(155, 28, 29);

				return;
			}

#undef background_fetch_block
#undef fetch_tile
#undef fetch_tile_name
#undef sprite_y_read
#undef sprite_fetch_block
#undef sprite_fetch_graphics
#undef sprite_fetch_coordinates
		}


/***********************************************
	Master System Fetching Code
************************************************/

		template<bool use_end> void fetch_sms(int start, int end) {
#define sprite_fetch(sprite)	{\
		line_buffer.active_sprites[sprite].x = \
			ram_[\
				master_system_.sprite_attribute_table_address & size_t(0x3f80 | (line_buffer.active_sprites[sprite].index << 1))\
			] - (master_system_.shift_sprites_8px_left ? 8 : 0);	\
		const uint8_t name = ram_[\
				master_system_.sprite_attribute_table_address & size_t(0x3f81 | (line_buffer.active_sprites[sprite].index << 1))\
			] & (sprites_16x16_ ? ~1 : ~0);\
		const size_t graphic_location = master_system_.sprite_generator_table_address & size_t(0x2000 | (name << 5) | (line_buffer.active_sprites[sprite].row << 2));	\
		line_buffer.active_sprites[sprite].image[0] = ram_[graphic_location];	\
		line_buffer.active_sprites[sprite].image[1] = ram_[graphic_location+1];	\
		line_buffer.active_sprites[sprite].image[2] = ram_[graphic_location+2];	\
		line_buffer.active_sprites[sprite].image[3] = ram_[graphic_location+3];	\
	}

#define sprite_fetch_block(location, sprite)	\
	slot(location):		\
	slot(location+1):	\
	slot(location+2):	\
	slot(location+3):	\
	slot(location+4):	\
	slot(location+5):	\
		sprite_fetch(sprite);\
		sprite_fetch(sprite+1);

#define sprite_y_read(location, sprite)	\
	slot(location):	\
		posit_sprite(sprite_selection_buffer, sprite, ram_[master_system_.sprite_attribute_table_address & ((sprite) | 0x3f00)], write_pointer_.row);	\
		posit_sprite(sprite_selection_buffer, sprite+1, ram_[master_system_.sprite_attribute_table_address & ((sprite + 1) | 0x3f00)], write_pointer_.row);	\

#define fetch_tile_name(column, row_info)	{\
		const size_t scrolled_column = (column - horizontal_offset) & 0x1f;\
		const size_t address = row_info.pattern_address_base + (scrolled_column << 1);	\
		line_buffer.names[column].flags = ram_[address+1];	\
		line_buffer.names[column].offset = size_t(	\
			(((line_buffer.names[column].flags&1) << 8) | ram_[address]) << 5	\
		) + row_info.sub_row[(line_buffer.names[column].flags&4) >> 2];	\
	}

#define fetch_tile(column)	\
	line_buffer.patterns[column][0] = ram_[line_buffer.names[column].offset];	\
	line_buffer.patterns[column][1] = ram_[line_buffer.names[column].offset+1];	\
	line_buffer.patterns[column][2] = ram_[line_buffer.names[column].offset+2];	\
	line_buffer.patterns[column][3] = ram_[line_buffer.names[column].offset+3];

#define background_fetch_block(location, column, sprite, row_info)	\
	slot(location):	fetch_tile_name(column, row_info)		\
	external_slot(location+1);					\
	slot(location+2):	\
	slot(location+3):	\
	slot(location+4):	\
		fetch_tile(column)					\
		fetch_tile_name(column+1, row_info)			\
		sprite_y_read(location+5, sprite);	\
	slot(location+6):	\
	slot(location+7):	\
	slot(location+8):	\
		fetch_tile(column+1)					\
		fetch_tile_name(column+2, row_info)				\
		sprite_y_read(location+9, sprite+2);	\
	slot(location+10):	\
	slot(location+11):	\
	slot(location+12):	\
		fetch_tile(column+2)					\
		fetch_tile_name(column+3, row_info)				\
		sprite_y_read(location+13, sprite+4);	\
	slot(location+14):	\
	slot(location+15): fetch_tile(column+3)

			// Determine the coarse horizontal scrolling offset; this isn't applied on the first two lines if the programmer has requested it.
			LineBuffer &line_buffer = line_buffers_[write_pointer_.row];
			LineBuffer &sprite_selection_buffer = line_buffers_[(write_pointer_.row + 1) % mode_timing_.total_lines];
			const int horizontal_offset = (write_pointer_.row >= 16 || !master_system_.horizontal_scroll_lock) ? (line_buffer.latched_horizontal_scroll >> 3) : 0;

			// Limit address bits in use if this is a SMS2 mode.
			const bool is_tall_mode = mode_timing_.pixel_lines != 192;
			const size_t pattern_name_address = master_system_.pattern_name_address | (is_tall_mode ? 0x800 : 0);
			const size_t pattern_name_offset = is_tall_mode ? 0x100 : 0;

			// Determine row info for the screen both (i) if vertical scrolling is applied; and (ii) if it isn't.
			// The programmer can opt out of applying vertical scrolling to the right-hand portion of the display.
			const int scrolled_row = (write_pointer_.row + master_system_.latched_vertical_scroll) % (is_tall_mode ? 256 : 224);
			struct RowInfo {
				size_t pattern_address_base;
				size_t sub_row[2];
			};
			const RowInfo scrolled_row_info = {
				(pattern_name_address & size_t(((scrolled_row & ~7) << 3) | 0x3800)) - pattern_name_offset,
				{size_t((scrolled_row & 7) << 2), 28 ^ size_t((scrolled_row & 7) << 2)}
			};
			RowInfo row_info;
			if(master_system_.vertical_scroll_lock) {
				row_info.pattern_address_base = (pattern_name_address & size_t(((write_pointer_.row & ~7) << 3) | 0x3800)) - pattern_name_offset;
				row_info.sub_row[0] = size_t((write_pointer_.row & 7) << 2);
				row_info.sub_row[1] = 28 ^ size_t((write_pointer_.row & 7) << 2);
			} else row_info = scrolled_row_info;

			// ... and do the actual fetching, which follows this routine:
			switch(start) {
				default: assert(false);

				sprite_fetch_block(0, 0);
				sprite_fetch_block(6, 2);

				external_slots_4(12);
				external_slot(16);

				sprite_fetch_block(17, 4);
				sprite_fetch_block(23, 6);

				slot(29):
					sprite_selection_buffer.reset_sprite_collection();
					do_external_slot(29*2);
				external_slot(30);

				sprite_y_read(31, 0);
				sprite_y_read(32, 2);
				sprite_y_read(33, 4);
				sprite_y_read(34, 6);
				sprite_y_read(35, 8);
				sprite_y_read(36, 10);
				sprite_y_read(37, 12);
				sprite_y_read(38, 14);

				background_fetch_block(39, 0, 16, scrolled_row_info);
				background_fetch_block(55, 4, 22, scrolled_row_info);
				background_fetch_block(71, 8, 28, scrolled_row_info);
				background_fetch_block(87, 12, 34, scrolled_row_info);
				background_fetch_block(103, 16, 40, scrolled_row_info);
				background_fetch_block(119, 20, 46, scrolled_row_info);
				background_fetch_block(135, 24, 52, row_info);
				background_fetch_block(151, 28, 58, row_info);

				external_slots_4(167);

				return;
			}

#undef background_fetch_block
#undef fetch_tile
#undef fetch_tile_name
#undef sprite_y_read
#undef sprite_fetch_block
#undef sprite_fetch
		}

#undef external_slot
#undef slot

		uint32_t *pixel_target_ = nullptr, *pixel_origin_ = nullptr;
		bool asked_for_write_area_ = false;
		void draw_tms_character(int start, int end);
		void draw_tms_text(int start, int end);
		void draw_sms(int start, int end, uint32_t cram_dot);
};

}
}

#endif /* TMS9918Base_hpp */
