//
//  Video.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 10/12/2024.
//  Copyright © 2024 Thomas Harte. All rights reserved.
//

#pragma once

namespace Commodore::Plus4 {

struct Video {
public:
	template <uint16_t address>
	void write(const uint8_t value) {
		const auto load_high10 = [&](uint16_t &target) {
			target = uint16_t(
				(target & 0x00ff) | ((value & 0x3) << 8)
			);
		};
		const auto load_low8 = [&](uint16_t &target) {
			target = uint16_t(
				(target & 0xff00) | value
			);
		};

		switch(address) {
			case 0xff06:
				extended_colour_mode_ = value & 0x40;
				bitmap_mode_ = value & 0x20;
				display_enable_ = value & 0x10;
				rows_25_ = value & 8;
				y_scroll_ = value & 7;
			break;

			case 0xff07:
				characters_256_ = value & 0x80;
				is_ntsc_ = value & 0x40;
				ted_off_ = value & 0x20;
				multicolour_mode_ = value & 0x10;
				columns_40_ = value & 8;
				x_scroll_ = value & 7;
			break;

			case 0xff12:
				character_generator_address_ = uint16_t((value & 0xfc) << 8);
			break;
			case 0xff14:
				screen_memory_address_ = uint16_t((value & 0xf8) << 8);
			break;

			case 0xff0c:	load_high10(cursor_address_);			break;
			case 0xff0d:	load_low8(cursor_address_);				break;
			case 0xff1a:	load_high10(character_row_address_);	break;
			case 0xff1b:	load_low8(character_row_address_);		break;
		}
	}

	// Outer clock is [NTSC or PAL] colour subcarrier * 2.
	//
	// 65 cycles = 64µs?
	// 65*262*60 = 1021800
	//
	// In an NTSC television system. 262 raster lines are produced (0 to 261), 312 for PAL (0−311).
	//
	// An interrupt is generated 8 cycles before the character window. For a 25 row display, the visible
	// raster lines are from 4 to 203.
	//
	// The horizontal position register counts 456 dots, 0 to 455.
	void run_for(Cycles) {
		// Timing:
		//
		//	456 cycles/line;
		//	if in PAL mode, divide input clock by 1.25 (?);
		//	see page 34 of plus4_tech.pdf for event times.

		// Horizontal events:
		//
		//	3: 38-column screen start
		//	288: external fetch window end, refresh single clock start, increment character position end
		//	290: latch character position to reload
		//	296: character window end, character window single clock end, increment refresh start
		//	304: video shift register end
		//	307: 38-column screen stop
		//	315: 40-column screen end
		//	328: refresh single clock end
		//	336: increment blink, increment refresh end
		//	344: horizontal blanking start
		//	358: horizontal sync start
		//	376: increment vertical line
		//	384: burst start, end of screen — clear vertical line, vertical sub and character reload registers
		//	390: horizontal sync end
		//	408: burst end
		//	400: external fetch window start
		//	416: horizontal blanking end
		//	424: increment character position reload
		//	432: character window start, character window single clock start, increment character position start
		//	440: video shift register start
		//	451: 40-column screen start

		// Vertical events:
		//
		//	0: attribute fetch start
		//	4: vertical screen window start (25 lines)
		//	8: vertical screen window start (24 lines)
		//	203: attribute fetch end
		//	200: vertical screen window stop (24 lines)
		//	204: frame window stop, vertical screen window stop (25 lines)
		//	226: NTSC vertical blank start
		//	229: NTSC vsync start
		//	232: NTSC vsync end
		//	244: NTSC vertical blank end
		//	251: PAL vertical blank start
		//	254: PAL vsync start
		//	257: PAL vsync end
		//	261: end of screen NTSC
		//	269: PAL vertical blank end
		//	311: end of screen PAL
	}

private:
	bool extended_colour_mode_ = false;
	bool bitmap_mode_ = false;
	bool display_enable_ = false;
	bool rows_25_ = false;
	int y_scroll_ = 0;

	bool characters_256_ = false;
	bool is_ntsc_ = false;
	bool ted_off_ = false;
	bool multicolour_mode_ = false;
	bool columns_40_ = false;
	int x_scroll_ = 0;

	uint16_t cursor_address_ = 0;
	uint16_t character_row_address_ = 0;
	uint16_t character_generator_address_ = 0;
	uint16_t screen_memory_address_ = 0;

	int horizontal_counter_ = 0;
	int vertical_counter_ = 0;
};

}
