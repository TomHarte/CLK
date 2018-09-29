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
	protected:
		Base(Personality p);

		Personality personality_;
		std::unique_ptr<Outputs::CRT::CRT> crt_;

		std::vector<uint8_t> ram_;

		uint16_t ram_pointer_ = 0;
		uint8_t read_ahead_buffer_ = 0;
		enum class MemoryAccess {
			Read, Write, None
		} queued_access_ = MemoryAccess::None;

		uint8_t status_ = 0;

		bool write_phase_ = false;
		uint8_t low_write_ = 0;

		// The various register flags.
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

		HalfCycles half_cycles_into_frame_;
		int column_ = 0, row_ = 0, output_column_ = 0;
		int cycles_error_ = 0;
		uint32_t *pixel_target_ = nullptr, *pixel_base_ = nullptr;

		void output_border(int cycles);

		// Vertical timing details.
		int frame_lines_ = 262;
		int first_vsync_line_ = 227;

		// Horizontal selections.
		enum class LineMode {
			Text,
			Character,
			Refresh,
			SMS
		} line_mode_ = LineMode::Text;
		int first_pixel_column_, first_right_border_column_;

		uint8_t pattern_names_[40];
		uint8_t pattern_buffer_[40];
		uint8_t colour_buffer_[40];

		struct SpriteSet {
			struct ActiveSprite {
				int index = 0;
				int row = 0;

				uint8_t info[4];
				uint8_t image[2];

				int shift_position = 0;
			} active_sprites[8];
			int active_sprite_slot = 0;
		} sprite_sets_[2];
		int active_sprite_set_ = 0;
		bool sprites_stopped_ = false;

		int access_pointer_ = 0;

		inline void test_sprite(int sprite_number, int screen_row);
		inline void get_sprite_contents(int start, int cycles, int screen_row);

		struct {
			bool vertical_scroll_lock = false;
			bool horizontal_scroll_lock = false;
			bool hide_left_column = false;
			bool enable_line_interrupts = false;
			bool shift_sprites_8px_left = false;
			bool mode4_enable = false;

			uint8_t colour_ram[32];

			struct {
				size_t offset;
				uint8_t flags;
			} names[32];
			uint8_t tile_graphics[32][4];
			size_t next_column = 0;
		} master_system_;

		enum class ScreenMode {
			Blank,
			Text,
			MultiColour,
			ColouredText,
			Graphics,
			SMSMode4
		} current_mode_;
		int height_ = 192;
		void set_current_mode() {
			if(blank_display_) {
				current_mode_ = ScreenMode::Blank;
				return;
			}

			if(is_sega_vdp(personality_) && master_system_.mode4_enable) {
				current_mode_ = ScreenMode::SMSMode4;
				height_ = 192;
				if(mode2_enable_ && mode1_enable_) height_ = 224;
				if(mode2_enable_ && mode3_enable_) height_ = 240;
				return;
			}

			if(!mode1_enable_ && !mode2_enable_ && !mode3_enable_) {
				current_mode_ = ScreenMode::ColouredText;
				return;
			}

			if(mode1_enable_ && !mode2_enable_ && !mode3_enable_) {
				current_mode_ = ScreenMode::Text;
				return;
			}

			if(!mode1_enable_ && mode2_enable_ && !mode3_enable_) {
				current_mode_ = ScreenMode::Graphics;
				return;
			}

			if(!mode1_enable_ && !mode2_enable_ && mode3_enable_) {
				current_mode_ = ScreenMode::MultiColour;
				return;
			}

			// TODO: undocumented TMS modes.
			current_mode_ = ScreenMode::Blank;
		}


		void external_slot() {
			// TODO: write or read a value if one is queued and ready to read/write.
			// (and, later: update the command engine, if this is an MSX2).
		}

#define slot(n)	\
		if(use_end && end+1 == n) return;\
		case n

#define external_slot(n)	\
	slot(n): external_slot()

		template<bool use_end> void fetch_sms(int start, int end) {
#define sprite_render_block(location, sprite)	\
	slot(location):		\
	slot(location+1):	\
	slot(location+2):	\
	slot(location+3):	\
	slot(location+4):	\
	slot(location+5):

/*
	TODO: sprite_render_block should fetch:
		- sprite n, x position and name
		- sprite n+1, x position and name
		- sprite n, tile graphic first word
		- sprite n, tile graphic second word
		- sprite n+1, tile graphic first word
		- sprite n+1, tile graphic second word
*/

#define sprite_y_read(location)	\
	slot(location):

/*
	TODO: sprite_y_read should fetch:
		- sprite n and n+1, y position
*/

#define fetch_tile_name(column)	{\
		size_t address = pattern_address_base + ((column) << 1);	\
		master_system_.names[column].flags = ram_[address+1];	\
		master_system_.names[column].offset = static_cast<size_t>((master_system_.names[column].flags&1 | ram_[address]) << 5) + sub_row;	\
	}

#define fetch_tile(column)	{\
		master_system_.tile_graphics[column][0] = ram_[master_system_.names[column].offset];	\
		master_system_.tile_graphics[column][1] = ram_[master_system_.names[column].offset+1];	\
		master_system_.tile_graphics[column][2] = ram_[master_system_.names[column].offset+2];	\
		master_system_.tile_graphics[column][3] = ram_[master_system_.names[column].offset+3];	\
	}

#define background_render_block(location, column)	\
	slot(location):	fetch_tile_name(column)		\
	external_slot(location+1);	\
	slot(location+2):	\
	slot(location+3): fetch_tile(column)	\
	slot(location+4): fetch_tile_name(column+1)	\
	sprite_y_read(location+5);	\
	slot(location+6):	\
	slot(location+7): fetch_tile(column+1)	\
	slot(location+8): fetch_tile_name(column+2)	\
	sprite_y_read(location+9);	\
	slot(location+10):	\
	slot(location+11): fetch_tile(column+2)	\
	slot(location+12): fetch_tile_name(column+3)	\
	sprite_y_read(location+13);	\
	slot(location+14):	\
	slot(location+15): fetch_tile(column+3)

/*
	TODO: background_render_block should fetch:
		- column n, name
		(external slot)
		- column n, tile graphic first word
		- column n, tile graphic second word
		- column n+1, name
		(sprite y fetch)
		- column n+1, tile graphic first word
		- column n+1, tile graphic second word
		- column n+2, name
		(sprite y fetch)
		- column n+2, tile graphic first word
		- column n+2, tile graphic second word
		- column n+3, name
		(sprite y fetch)
		- column n+3, tile graphic first word
		- column n+3, tile graphic second word
*/
			const size_t pattern_address_base = (pattern_name_address_ | size_t(0x3ff)) & static_cast<size_t>(((row_ & ~7) << 6) | 0x3800);
			const size_t sub_row = static_cast<size_t>((row_ & 7) << 2);

			switch(start) {
				default:
				sprite_render_block(0, 0);
				sprite_render_block(6, 2);
				external_slot(12);
				external_slot(13);
				external_slot(14);
				external_slot(15);
				external_slot(16);
				sprite_render_block(17, 4);
				sprite_render_block(23, 6);
				external_slot(29);
				external_slot(30);
				sprite_y_read(31);
				sprite_y_read(32);
				sprite_y_read(33);
				sprite_y_read(34);
				sprite_y_read(35);
				sprite_y_read(36);
				sprite_y_read(37);
				sprite_y_read(38);
				background_render_block(39, 0);
				background_render_block(55, 4);
				background_render_block(71, 8);
				background_render_block(87, 12);
				background_render_block(103, 16);
				background_render_block(119, 20);
				background_render_block(135, 24);
				background_render_block(151, 28);
				external_slot(167);
				external_slot(168);
				external_slot(169);
				external_slot(170);

				return;
			}

#undef background_render_block
#undef sprite_y_read
#undef sprite_render_block
		}

#undef external_slot
#undef slot

};

}
}

#endif /* TMS9918Base_hpp */
