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

#include <cstdint>
#include <memory>

namespace TI {
namespace TMS {

enum Personality {
	TMS9918A,	// includes the 9928 and 9929; set TV standard and output device as desired.
	V9938,
	V9958,
	SMSVDP,
	GGVDP,
};

#define is_sega_vdp(x) x >= SMSVDP

class Base {
	public:
		static const uint32_t palette_pack(uint8_t r, uint8_t g, uint8_t b) {
			uint32_t result = 0;
			uint8_t *const result_ptr = reinterpret_cast<uint8_t *>(&result);
			result_ptr[0] = r;
			result_ptr[1] = g;
			result_ptr[2] = b;
			result_ptr[3] = 0;
			return result;
		}

	protected:
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

		Personality personality_;
		std::unique_ptr<Outputs::CRT::CRT> crt_;

		// Holds the contents of this VDP's connected DRAM.
		std::vector<uint8_t> ram_;

		// Holds the state of the DRAM/CRAM-access mechanism.
		uint16_t ram_pointer_ = 0;
		uint8_t read_ahead_buffer_ = 0;
		enum class MemoryAccess {
			Read, Write, None
		} queued_access_ = MemoryAccess::None;

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

		size_t pattern_name_address_ = 0;
		size_t colour_table_address_ = 0;
		size_t pattern_generator_table_address_ = 0;
		size_t sprite_attribute_table_address_ = 0;
		size_t sprite_generator_table_address_ = 0;

		uint8_t text_colour_ = 0;
		uint8_t background_colour_ = 0;

		// Internal mechanisms for position tracking.
		HalfCycles half_cycles_into_frame_;
		int column_ = 0, row_ = 0, output_column_ = 0;
		int cycles_error_ = 0;
		int access_pointer_ = 0;

		// A helper function to output the current border colour for
		// the number of cycles supplied.
		void output_border(int cycles);

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
			int first_pixel_output_column;
			int next_border_column;

			// Maximum number of sprite slots to populate;
			// if sprites beyond this number should be visible
			// then the appropriate status information will be set.
			int maximum_visible_sprites = 4;

			//
			int end_of_frame_interrupt_position = 342;
			int line_interrupt_position = -1;

			bool allow_sprite_terminator = true;
		} mode_timing_;

		uint8_t line_interrupt_target = 0;
		uint8_t line_interrupt_counter = 0;
		bool enable_line_interrupts_ = false;
		bool line_interrupt_pending_ = false;

		// The line mode describes the proper timing diagram for the current line.
		enum class LineMode {
			Text,
			Character,
			Refresh,
			SMS
		} line_mode_ = LineMode::Text;

		// Temporary buffers collect a representation of this line prior to pixel serialisation.
		uint8_t pattern_names_[40];
		uint8_t pattern_buffer_[40];
		uint8_t colour_buffer_[40];

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

			// Temporary buffers for a line of Master System graphics.
			struct {
				size_t offset;
				uint8_t flags;
			} names[32];
			uint8_t tile_graphics[32][4];
			size_t next_column = 0;
		} master_system_;

		// Holds results of sprite data fetches that occur on this
		// line. Therefore has to contain: up to four or eight sets
		// of sprite data for this line, and its horizontal position,
		// plus a growing list of which sprites are selected for
		// the next line.
		struct SpriteSet {
			struct ActiveSprite {
				int index = 0;
				int row = 0;

				uint8_t image[4];
				int shift_position = 0;
			} active_sprites[8];

			int active_sprite_slot = 0;
			int fetched_sprite_slot = 0;
			bool sprites_stopped = false;
		} sprite_set_;

		inline void reset_sprite_collection();
		inline void posit_sprite(int sprite_number, int sprite_y, int screen_row);
		inline void get_sprite_contents(int start, int cycles, int screen_row);

		enum class ScreenMode {
			Blank,
			Text,
			MultiColour,
			ColouredText,
			Graphics,
			SMSMode4
		} screen_mode_;
		void set_current_mode() {
			if(blank_display_) {
				screen_mode_ = ScreenMode::Blank;
				return;
			}

			if(is_sega_vdp(personality_) && master_system_.mode4_enable) {
				screen_mode_ = ScreenMode::SMSMode4;
				mode_timing_.pixel_lines = 192;
				if(mode2_enable_ && mode1_enable_) mode_timing_.pixel_lines = 224;
				if(mode2_enable_ && mode3_enable_) mode_timing_.pixel_lines = 240;
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


		void do_external_slot() {
			// TODO: write or read a value if one is queued and ready to read/write.
			// (and, later: update the command engine, if this is an MSX2).
			switch(queued_access_) {
				default: return;

				case MemoryAccess::Write:
					if(master_system_.cram_is_selected) {
						master_system_.colour_ram[ram_pointer_ & 0x1f] = palette_pack(
							static_cast<uint8_t>(((read_ahead_buffer_ >> 0) & 3) * 255 / 3),
							static_cast<uint8_t>(((read_ahead_buffer_ >> 2) & 3) * 255 / 3),
							static_cast<uint8_t>(((read_ahead_buffer_ >> 4) & 3) * 255 / 3)
						);
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

#define slot(n)	\
		if(use_end && end+1 == n) return;\
		case n

#define external_slot(n)	\
	slot(n): do_external_slot();

#define external_slots_2(n)	\
	external_slot(n);	\
	external_slot(n+1);

#define external_slots_4(n)	\
	external_slots_2(n);	\
	external_slots_2(n+2);

#define external_slots_8(n)	\
	external_slots_4(n);	\
	external_slots_4(n+4);

#define external_slots_16(n)	\
	external_slots_8(n);	\
	external_slots_8(n+8);

#define external_slots_32(n)	\
	external_slots_16(n);	\
	external_slots_16(n+16);

/*
	Fetching routines follow below; they obey the following rules:

		1) 	input is a start position and an end position; they should perform the proper
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

		* 	the function external_slot(), which will perform any pending VRAM read/write.
		*	the macros slot(n) and external_slot(n) which can be used to schedule those things inside a
			switch(start)-based implementation.

	All functions should just spool data to intermediary storage. This is because for most VDPs there is
	a decoupling between fetch pattern and output pattern, and it's neater to keep the same division
	for the exceptions.
*/


/***********************************************
             TMS9918 Fetching Code
************************************************/

		template<bool use_end> void fetch_tms_refresh(int start, int end) {
#define refresh(location)	external_slot(location+1)

#define refreshes_2(location)	\
	refresh(location);	\
	refresh(location+2);

#define refreshes_4(location)	\
	refreshes_2(location);	\
	refreshes_2(location+4);

#define refreshes_8(location)	\
	refreshes_4(location);	\
	refreshes_4(location+8);

			switch(start) {
				default:

				/* 44 external slots (= 44 windows) */
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
#define fetch_tile_name(location, column)		slot(location): pattern_names_[column] = ram_[row_base + column];
#define fetch_tile_pattern(location, column)	slot(location): pattern_buffer_[column] = ram_[row_offset + static_cast<size_t>(pattern_names_[column] << 3)];

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

			const size_t row_base = pattern_name_address_ + static_cast<size_t>(row_ >> 3) * 40;
			const size_t row_offset = pattern_generator_table_address_ + (row_ & 7);

			switch(start) {
				default:

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

					/* 4 more external slots */
					external_slots_4(167);

				return;
			}

#undef fetch_columns_8
#undef fetch_columns_4
#undef fetch_columns_2
#undef fetch_column
#undef fetch_tile_name
		}

		template<bool use_end> void fetch_tms_character(int start, int end) {
#define sprite_fetch_coordinates(location, sprite)	\
	slot(location):		\
	slot(location+1):	\

#define sprite_fetch_graphics(location, sprite)	\
	slot(location):		\
	slot(location+1):	\
	slot(location+2):	\
	slot(location+3):	\

#define sprite_fetch_block(location, sprite)	\
	slot(location):		\
	slot(location+1):	\
	slot(location+2):	\
	slot(location+3):	\
	slot(location+4):	\
	slot(location+5):

#define sprite_y_read(location, sprite)	\
	slot(location):

#define fetch_tile_name(column) pattern_names_[column] = ram_[(row_base + column) & 0x3fff];

#define fetch_tile(column)	{\
		colour_buffer_[column] = ram_[(colour_base + static_cast<size_t>((pattern_names_[column] << 3) >> colour_name_shift)) & 0x3fff];		\
		pattern_buffer_[column] = ram_[(pattern_base + static_cast<size_t>(pattern_names_[column] << 3)) & 0x3fff];	\
	}

#define background_fetch_block(location, column)	\
	slot(location):	fetch_tile_name(column)		\
	external_slot(location+1);	\
	slot(location+2):	\
	slot(location+3): fetch_tile(column)	\
	slot(location+4): fetch_tile_name(column+1)	\
	sprite_y_read(location+5, column+8);	\
	slot(location+6):	\
	slot(location+7): fetch_tile(column+1)	\
	slot(location+8): fetch_tile_name(column+2)	\
	sprite_y_read(location+9, column+9);	\
	slot(location+10):	\
	slot(location+11): fetch_tile(column+2)	\
	slot(location+12): fetch_tile_name(column+3)	\
	sprite_y_read(location+13, column+10);	\
	slot(location+14):	\
	slot(location+15): fetch_tile(column+3)

			const size_t row_base = pattern_name_address_ + static_cast<size_t>((row_ << 2)&~31);

			size_t pattern_base = pattern_generator_table_address_;
			size_t colour_base = colour_table_address_;
			int colour_name_shift = 6;

			if(screen_mode_ == ScreenMode::Graphics) {
				// If this is high resolution mode, allow the row number to affect the pattern and colour addresses.
				pattern_base &= static_cast<size_t>(0x2000 | ((row_ & 0xc0) << 5));
				colour_base &= static_cast<size_t>(0x2000 | ((row_ & 0xc0) << 5));

				colour_base += static_cast<size_t>(row_ & 7);
				colour_name_shift = 0;
			}

			if(screen_mode_ == ScreenMode::MultiColour) {
				pattern_base += static_cast<size_t>((row_ >> 2) & 7);
			} else {
				pattern_base += static_cast<size_t>(row_ & 7);
			}

			switch(start) {
				default:
				external_slots_2(0);

				sprite_fetch_block(2, 0);
				sprite_fetch_block(8, 1);
				sprite_fetch_coordinates(14, 2);

				external_slots_4(16);
				external_slot(20);

				sprite_fetch_graphics(21, 2);
				sprite_fetch_block(25, 3);

				external_slots_4(31);

				sprite_y_read(35, 0);
				sprite_y_read(36, 1);
				sprite_y_read(37, 2);
				sprite_y_read(38, 3);
				sprite_y_read(39, 4);
				sprite_y_read(40, 5);
				sprite_y_read(41, 6);
				sprite_y_read(42, 7);

				background_fetch_block(43, 0);
				background_fetch_block(59, 4);
				background_fetch_block(75, 8);
				background_fetch_block(91, 12);
				background_fetch_block(107, 16);
				background_fetch_block(123, 20);
				background_fetch_block(139, 24);
				background_fetch_block(155, 28);

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
		sprite_set_.active_sprites[sprite].shift_position = -ram_[sprite_attribute_table_address_ + 128 + (sprite_set_.active_sprites[sprite].index << 1)] + (master_system_.shift_sprites_8px_left ? 8 : 0);	\
		sprite_set_.active_sprites[sprite].image[0] =	\
		sprite_set_.active_sprites[sprite].image[1] =	\
		sprite_set_.active_sprites[sprite].image[2] =	\
		sprite_set_.active_sprites[sprite].image[3] = 0xff;	\
	}

//		size_t graphic_location = sprite_generator_table_address_;	\

#define sprite_fetch_block(location, sprite)	\
	slot(location):		\
	slot(location+1):	\
	slot(location+2):	\
	slot(location+3):	\
	slot(location+4):	\
	slot(location+5):	\
		sprite_fetch(sprite);\
		sprite_fetch(sprite+1);

/*
	TODO: sprite_render_block should fetch:
		- sprite n, x position and name
		- sprite n+1, x position and name
		- sprite n, tile graphic first word
		- sprite n, tile graphic second word
		- sprite n+1, tile graphic first word
		- sprite n+1, tile graphic second word
*/

#define sprite_y_read(location, sprite)	\
	slot(location):	\
		posit_sprite(sprite, ram_[sprite_attribute_table_address_ + sprite], row_);	\
		posit_sprite(sprite, ram_[sprite_attribute_table_address_ + sprite + 1], row_);	\

#define fetch_tile_name(column)	{\
		const size_t scrolled_column = (column - horizontal_offset) & 0x1f;\
		const size_t address = pattern_address_base + (scrolled_column << 1);	\
		master_system_.names[column].flags = ram_[address+1];	\
		master_system_.names[column].offset = static_cast<size_t>(	\
			(((master_system_.names[column].flags&1) << 8) | ram_[address]) << 5	\
		) + sub_row[(master_system_.names[column].flags&4) >> 2];	\
	}

#define fetch_tile(column)	\
	master_system_.tile_graphics[column][0] = ram_[master_system_.names[column].offset];	\
	master_system_.tile_graphics[column][1] = ram_[master_system_.names[column].offset+1];	\
	master_system_.tile_graphics[column][2] = ram_[master_system_.names[column].offset+2];	\
	master_system_.tile_graphics[column][3] = ram_[master_system_.names[column].offset+3];

#define background_render_block(location, column, sprite)	\
	slot(location):	fetch_tile_name(column)		\
	external_slot(location+1);					\
	slot(location+2):	\
	slot(location+3):	\
	slot(location+4):	\
		fetch_tile(column)					\
		fetch_tile_name(column+1)			\
		sprite_y_read(location+5, sprite);	\
	slot(location+6):	\
	slot(location+7): 	\
	slot(location+8):	\
		fetch_tile(column+1)					\
		fetch_tile_name(column+2)				\
		sprite_y_read(location+9, sprite+2);	\
	slot(location+10):	\
	slot(location+11):	\
	slot(location+12): 	\
		fetch_tile(column+2)					\
		fetch_tile_name(column+3)				\
		sprite_y_read(location+13, sprite+4);	\
	slot(location+14):	\
	slot(location+15): fetch_tile(column+3)

			const int scrolled_row = (row_ + master_system_.vertical_scroll) % 224;
			const size_t pattern_address_base = (pattern_name_address_ | size_t(0x3ff)) & static_cast<size_t>(((scrolled_row & ~7) << 3) | 0x3800);
			const size_t sub_row[2] = {static_cast<size_t>((scrolled_row & 7) << 2), 28 ^ static_cast<size_t>((scrolled_row & 7) << 2)};
			const int horizontal_offset = (row_ >= 16 || !master_system_.horizontal_scroll_lock) ? (master_system_.horizontal_scroll >> 3) : 0;

			/*
				To add, relative to the times below:

					hsync active at cycle 14;
					hsync inactive at cycle 27;
			*/

			switch(start) {
				default:
				external_slots_4(0);

				sprite_fetch_block(4, 0);
				sprite_fetch_block(10, 2);

				external_slots_4(16);
				external_slot(20);

				sprite_fetch_block(21, 4);
				sprite_fetch_block(27, 6);

				slot(33):
					reset_sprite_collection();
					do_external_slot();
				external_slot(34);

				sprite_y_read(35, 0);
				sprite_y_read(36, 2);
				sprite_y_read(37, 4);
				sprite_y_read(38, 6);
				sprite_y_read(39, 8);
				sprite_y_read(40, 10);
				sprite_y_read(41, 12);
				sprite_y_read(42, 14);

				background_render_block(43, 0, 16);
				background_render_block(59, 4, 22);
				background_render_block(75, 8, 28);
				background_render_block(91, 12, 34);
				background_render_block(107, 16, 40);
				background_render_block(123, 20, 46);
				background_render_block(139, 24, 52);	// TODO: this and the next one should ignore master_system_.vertical_scroll.
				background_render_block(156, 28, 58);

				return;
			}

#undef background_render_block
#undef sprite_y_read
#undef sprite_render_block
		}

#undef external_slot
#undef slot

		uint32_t *pixel_target_ = nullptr, *pixel_origin_ = nullptr;
		void draw_tms_character(int start, int end);
		void draw_tms_text(int start, int end);
		void draw_sms(int start, int end);

};

}
}

#endif /* TMS9918Base_hpp */
