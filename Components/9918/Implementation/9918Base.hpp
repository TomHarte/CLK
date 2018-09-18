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

class Base {
	protected:
		Base(Personality p);

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
		int next_screen_mode_ = 0, screen_mode_ = 0;
		bool next_blank_screen_ = true, blank_screen_ = true;
		bool sprites_16x16_ = false;
		bool sprites_magnified_ = false;
		bool generate_interrupts_ = false;
		int sprite_height_ = 8;
		uint16_t pattern_name_address_ = 0;
		uint16_t colour_table_address_ = 0;
		uint16_t pattern_generator_table_address_ = 0;
		uint16_t sprite_attribute_table_address_ = 0;
		uint16_t sprite_generator_table_address_ = 0;

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
			} active_sprites[4];
			int active_sprite_slot = 0;
		} sprite_sets_[2];
		int active_sprite_set_ = 0;
		bool sprites_stopped_ = false;

		int access_pointer_ = 0;

		inline void test_sprite(int sprite_number, int screen_row);
		inline void get_sprite_contents(int start, int cycles, int screen_row);

		// Contains tables describing the memory access patterns and, implicitly,
		// the timing of video generation.
		enum Operation {
			HSyncOn,
			HSyncOff,
			ColourBurstOn,
			ColourBurstOff,

			/// A memory access slot that is available for an external read or write.
			External,

			/// A refresh cycle; neither used for video fetching nor available for external use.
			Refresh,

			/*!
				Column N Name Table Read
					[= 1 slot]
			*/
			NameTableRead,

			/*!
				Column N Pattern Table Read
					[= 1 slot]
			*/
			PatternTableRead,

			/*!
				Y0, X0, N0, C0, Pattern 0 (1), Pattern 0 (2),
				Y1, X1, N1, C1, Pattern 1 (1), Pattern 1 (2),
				Y2, X2
					[= 14 slots]
			*/
			TMSSpriteFetch1,

			/*!
				N2, C2, Pattern 2 (1), Pattern 2 (2),
				Y3, X3, N3, C3, Pattern 3 (1), Pattern 3 (2),
					[= 10 slots]
			*/
			TMSSpriteFetch2,

			/*!
				Sprite N fetch, Sprite N+1 fetch [...]
			*/
			TMSSpriteYFetch,

			/*!
				Colour N, Pattern N,
				Name N+1,
				Sprite N,

				Colour N+1, Pattern N+1,
				Name N+2,
				Sprite N+1,

				Colour N+2, Pattern N+2,
				Name N+3,
				Sprite N+2,

				Colour N+3, Pattern N+3,
				Name N+4,
					[= 15 slots]
			*/
			TMSBackgroundRenderBlock,

			/*!
				Pattern N,
				Name N+1
			*/
			TMSPatternNameFetch,

			/*!
				Sprite N X/Name Read
				Sprite N+1 X/Name Read
				Sprite N Tile read (1st word)
				Sprite N Tile read (2nd word)
				Sprite N+1 Tile Read (1st word)
				Sprite N+1 Tile Read (2nd word)
					[= 6 slots]
			*/
			SMSSpriteRenderBlock,

			/*!
				Column N Tile Read (1st word)
				Column N Tile Read (2nd word)
				Column N+1 Name Table Read
				Sprite (16+N*1.5) Y Read (Reads Y of 2 sprites)
				Column N+1 Tile Read (1st word)
				Column N+1 Tile Read (2nd word)
				Column N+2 Name Table Read
				Sprite (16+N*1.5+2) Y Read (Reads Y of 2 sprites)
				Column N+2 Tile Read (1st word)
				Column N+2 Tile Read (2nd word)
				Column N+3 Name Table Read
				Sprite (16+N*1.5+4) Y Read (Reads Y of 2 sprites)
				Column N+3 Tile Read (1st word)
				Column N+3 Tile Read (2nd word)
					[= 14 slots]
			*/
			SMSBackgroundRenderBlock,
		};
		struct Period {
			Operation operation;
			int duration;
		};
};

}
}

#endif /* TMS9918Base_hpp */
