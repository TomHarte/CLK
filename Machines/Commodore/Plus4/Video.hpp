//
//  Video.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 10/12/2024.
//  Copyright © 2024 Thomas Harte. All rights reserved.
//

#pragma once

#include "../../../Numeric/UpperBound.hpp"

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
	void run_for(Cycles cycles) {
		// Timing:
		//
		//	456 cycles/line;
		//	if in PAL mode, divide input clock by 1.25 (?);
		//	see page 34 of plus4_tech.pdf for event times.

		auto ticks_remaining = cycles.as<int>() * 8;
		while(ticks_remaining) {
			// Test vertical first; this will catch both any programmed change that has occurred outside
			// of the loop and any change to the vertical counter that occurs during the horizontal runs.
			const auto attribute_fetch_start = []{};
			switch(vertical_counter_) {
				case 261:	// End of screen NTSC. [and hence 0: Attribute fetch start].
					if(is_ntsc_) {
						vertical_counter_ = 0;
						attribute_fetch_start();
					}
				break;

				case 311:	// End of screen PAL. [and hence 0: Attribute fetch start].
					if(!is_ntsc_) {
						vertical_counter_ = 0;
						attribute_fetch_start();
					}
				break;

				case 4:		// Vertical screen window start (25 lines).
				break;

				case 8:		// Vertical screen window start (24 lines).
				break;

				case 200:	// Vertical screen window stop (24 lines).
				break;

				case 203:	// Attribute fetch end.
				break;

				case 204:	// Vertical screen window stop (25 lines).
				break;

				case 226:	// NTSC vertical blank start.
				break;

				case 229:	// NTSC vsync start.
				break;

				case 232:	// NTSC vsync end.
				break;

				case 244:	// NTSC vertical blank end.
				break;

				case 251:	// PAL vertical blank start.
				break;

				case 254:	// PAL vsync start.
				break;

				case 257:	// PAL vsync end.
				break;

				case 269:	// PAL vertical blank end.
				break;
			}

			const auto next = Numeric::upper_bound<
				0, 3, 288, 290, 296, 304, 307, 315, 328, 336, 344, 358, 376, 384, 390, 400, 416, 424, 432, 440, 451, 465
			>(horizontal_counter_);
			const auto period = std::min(next - horizontal_counter_, ticks_remaining);

//			printf("From %d next is %d\n", horizontal_counter_, next);

			horizontal_counter_ += period;
			ticks_remaining -= period;
			switch(horizontal_counter_) {
				case 3:		// 38-column screen start.
				break;

				case 288:	// External fetch window end, refresh single clock start, increment character position end.
				break;

				case 290:	// Latch character position to reload.
				break;

				case 296:	// Character window end, character window single clock end, increment refresh start.
				break;

				case 304:	// Video shift register end.
				break;

				case 307:	// 38-column screen stop.
				break;

				case 315:	// 40-column screen end.
				break;

				case 328:	// Refresh single clock end.
				break;

				case 336:	// Increment blink, increment refresh end.
				break;

				case 344:	// Horizontal blanking start.
				break;

				case 358:	// Horizontal sync start.
				break;

				case 376:	// Increment vertical line.
					++vertical_counter_;
				break;

				case 384:	// Burst start, end of screen — clear vertical line, vertical sub and character reload registers.
				break;

				case 390:	// Horizontal sync end.
				break;

				case 400:	// External fetch window start.
				break;

				case 408:	// Burst end.
				break;

				case 416:	// Horizontal blanking end.
				break;

				case 424:	// Increment character position reload.
				break;

				case 432:	// Character window start, character window single clock start, increment character position start.
				break;

				case 440:	// Video shift register start.
				break;

				case 451:	// 40-column screen start.
				break;

				case 465:	// Wraparound.
					horizontal_counter_ = 0;
				break;
			}
		}
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
