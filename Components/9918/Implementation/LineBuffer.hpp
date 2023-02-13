//
//  LineBuffer.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 12/02/2023.
//  Copyright © 2023 Thomas Harte. All rights reserved.
//

#ifndef LineBuffer_hpp
#define LineBuffer_hpp

#include "AccessEnums.hpp"

namespace TI {
namespace TMS {

// Temporary buffers collect a representation of each line prior to pixel serialisation.
struct LineBuffer {
	LineBuffer() {}

	// The fetch mode describes the proper timing diagram for this line;
	// screen mode captures proper output mode.
	FetchMode fetch_mode = FetchMode::Text;
	ScreenMode screen_mode = ScreenMode::Text;
	VerticalState vertical_state = VerticalState::Blank;

	// Holds the horizontal scroll position to apply to this line;
	// of those VDPs currently implemented, affects the Master System only.
	uint8_t latched_horizontal_scroll = 0;

	// The names array holds pattern names, as an offset into memory, and
	// potentially flags also.
	union {
		// The TMS and Sega VDPs are close enough to always tile-based;
		// this struct captures maximal potential detail there.
		struct {
			uint8_t flags[40]{};

			// The patterns array holds tile patterns, corresponding 1:1 with names.
			// Four bytes per pattern is the maximum required by any
			// currently-implemented VDP.
			uint8_t patterns[40][4]{};
		};

		// The Yamaha VDP also has a variety of bitmap modes,
		// the widest of which is 512px @ 4bpp.
		uint8_t bitmap[256];
	};

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
	int pixel_count = 256;

	// An active sprite is one that has been selected for composition onto
	// _this_ line.
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
};

struct LineBufferPointer {
	int row = 0, column = 0;
};

}
}

#endif /* LineBuffer_hpp */
