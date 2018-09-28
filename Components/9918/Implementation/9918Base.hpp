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
			Text = 0,
			Character = 1,
			Refresh = 2
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

/*
#define slot(n)	\
		if(use_end && end+1 == n) return;\
		case n

		template<bool use_end> void fetch_sms(int start, int end) {
#define sprite_render_block(location, sprite)	\
	slot(location):	\
		sprite_sets_[active_sprite_set_].info[0] =
			switch(start) {
				default:
//				slot(1):

				return;
			}
		}

#undef slot
*/

//		// Contains tables describing the memory access patterns and, implicitly,
//		// the timing of video generation.
//		enum Operation {
//			HSyncOn,
//			HSyncOff,
//			ColourBurstOn,
//			ColourBurstOff,
//
//			/// A memory access slot that is available for an external read or write.
//			External,
//
//			/// A refresh cycle; neither used for video fetching nor available for external use.
//			Refresh,
//
//			/// A slot that reads the next sprite location for
//			ReadSpriteY,
//
//			/*!
//				Column N Name Table Read
//					[= 1 slot]
//			*/
//			NameTableRead,
//
//			/*!
//				Column N Pattern Table Read
//					[= 1 slot]
//			*/
//			PatternTableRead,
//
//			/*!
//				Y0, X0, N0, C0, Pattern 0 (1), Pattern 0 (2),
//				Y1, X1, N1, C1, Pattern 1 (1), Pattern 1 (2),
//				Y2, X2
//					[= 14 slots]
//			*/
//			TMSSpriteFetch1,
//
//			/*!
//				N2, C2, Pattern 2 (1), Pattern 2 (2),
//				Y3, X3, N3, C3, Pattern 3 (1), Pattern 3 (2),
//					[= 10 slots]
//			*/
//			TMSSpriteFetch2,
//
//			/*!
//				Sprite N fetch, Sprite N+1 fetch [...]
//			*/
//			TMSSpriteYFetch,
//
//			/*!
//				Colour N, Pattern N,
//				Name N+1,
//					[= 3 slots]
//			*/
//			TMSBackgroundRenderBlock,
//
//			/*!
//				Colour N, Pattern N,
//			*/
//			TMSColourPatternFetch,
//
//			/*!
//				Sprite N X/Name Read
//				Sprite N+1 X/Name Read
//				Sprite N Tile read (1st word)
//				Sprite N Tile read (2nd word)
//				Sprite N+1 Tile Read (1st word)
//				Sprite N+1 Tile Read (2nd word)
//					[= 6 slots]
//			*/
//			SMSSpriteRenderBlock,
//
//			/*!
//				Column N Tile Read (1st word)
//				Column N Tile Read (2nd word)
//				Column N+1 Name Table Read
//				Sprite (16+N*1.5) Y Read (Reads Y of 2 sprites)
//				Column N+1 Tile Read (1st word)
//				Column N+1 Tile Read (2nd word)
//				Column N+2 Name Table Read
//				Sprite (16+N*1.5+2) Y Read (Reads Y of 2 sprites)
//				Column N+2 Tile Read (1st word)
//				Column N+2 Tile Read (2nd word)
//				Column N+3 Name Table Read
//				Sprite (16+N*1.5+4) Y Read (Reads Y of 2 sprites)
//				Column N+3 Tile Read (1st word)
//				Column N+3 Tile Read (2nd word)
//					[= 14 slots]
//			*/
//			SMSBackgroundRenderBlock,
//		};
//		struct Period {
//			Operation operation;
//			int duration;
//		};
};

}
}

#endif /* TMS9918Base_hpp */
