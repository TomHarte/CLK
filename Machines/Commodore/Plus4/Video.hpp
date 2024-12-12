//
//  Video.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 10/12/2024.
//  Copyright © 2024 Thomas Harte. All rights reserved.
//

#pragma once

#include "Pager.hpp"

#include "../../../Numeric/UpperBound.hpp"
#include "../../../Outputs/CRT/CRT.hpp"

namespace Commodore::Plus4 {

struct Video {
public:
	Video(const Commodore::Plus4::Pager &pager) :
		crt_(465, 1, Outputs::Display::Type::PAL50, Outputs::Display::InputDataType::Luminance8Phase8),
		pager_(pager) {}

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

//		printf("bitmap:%d c256:%d ntsc:%d 40col:%d; base:%04x\n", bitmap_mode_, characters_256_, is_ntsc_, columns_40_, screen_memory_address_);
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

		subcycles_ += cycles * 4;
		auto ticks_remaining = subcycles_.divide(is_ntsc_ ? Cycles(4) : Cycles(5)).as<int>();
		while(ticks_remaining) {
			//
			// Test vertical first; this will catch both any programmed change that has occurred outside
			// of the loop and any change to the vertical counter that occurs during the horizontal runs.
			//
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

				case 203:	// Attribute fetch end.
				break;

				case 4:		if(rows_25_) vertical_window_ = true;	break;	// Vertical screen window start (25 lines).
				case 204:	if(rows_25_) vertical_window_ = false;	break;	// Vertical screen window stop (25 lines).
				case 8:		if(!rows_25_) vertical_window_ = true;	break;	// Vertical screen window start (24 lines).
				case 200:	if(!rows_25_) vertical_window_ = false;	break;	// Vertical screen window stop (24 lines).

				case 226:	if(is_ntsc_) vertical_blank_ = true;	break;	// NTSC vertical blank start.
				case 229:	if(is_ntsc_) vertical_sync_ = true;		break;	// NTSC vsync start.
				case 232:	if(is_ntsc_) vertical_sync_ = false;	break;	// NTSC vsync end.
				case 244:	if(is_ntsc_) vertical_blank_ = false;	break;	// NTSC vertical blank end.

				case 251:	if(!is_ntsc_) vertical_blank_ = true;	break;	// PAL vertical blank start.
				case 254:	if(!is_ntsc_) vertical_sync_ = true;	break;	// PAL vsync start.
				case 257:	if(!is_ntsc_) vertical_sync_ = false;	break;	// PAL vsync end.
				case 269:	if(!is_ntsc_) vertical_blank_ = false;	break;	// PAL vertical blank end.
			}

			const auto next = Numeric::upper_bound<
				0, 3, 288, 290, 296, 304, 307, 315, 328, 336, 344, 358, 376, 384, 390, 400, 416, 424, 432, 440, 451, 465
			>(horizontal_counter_);
			const auto period = std::min(next - horizontal_counter_, ticks_remaining);

			//
			// Output.
			//
			OutputState state;
			if(vertical_sync_ || horizontal_sync_) {
				state = OutputState::Sync;
			} else if(vertical_blank_ || horizontal_blank_) {
				state = horizontal_burst_ ? OutputState::Burst : OutputState::Blank;
			} else {
				state = vertical_window_ && output_pixels_ ? OutputState::Pixels : OutputState::Border;
			}

			if(state != output_state_) {
				switch(output_state_) {
					case OutputState::Blank:	crt_.output_blank(time_in_state_);						break;
					case OutputState::Sync:		crt_.output_sync(time_in_state_);						break;
					case OutputState::Burst:	crt_.output_default_colour_burst(time_in_state_);		break;
					case OutputState::Border:	crt_.output_level<uint16_t>(time_in_state_, 0x8080);	break;
					case OutputState::Pixels:	crt_.output_level<uint16_t>(time_in_state_, 0xff80);	break;
				}
				time_in_state_ = 0;
			}
			output_state_ = state;
			time_in_state_ += period;

			//
			// Advance for current period.
			//
			horizontal_counter_ += period;
			ticks_remaining -= period;
			switch(horizontal_counter_) {
				case 288:	// External fetch window end, refresh single clock start, increment character position end.
					// TODO: release RDY if it was held.
					// TODO: increment character position end.
					refresh_ = true;
				break;

				case 400:	// External fetch window start.
					// TODO: set RDY line if this is an appropriate row.
				break;

				case 290:	line_character_address_ = character_address_;	break;	// Latch character position to reload.

				case 296:	// Character window end, character window single clock end, increment refresh start.
					fetch_characters_ = false;
				break;
				case 432:	// Character window start, character window single clock start, increment character position start.
					fetch_characters_ = true;
				break;

				case 304:	// Video shift register end.
				break;

				case 3:		if(!columns_40_) output_pixels_ = true;		break;	// 38-column screen start.
				case 307:	if(!columns_40_) output_pixels_ = false;	break;	// 38-column screen stop.
				case 451:	if(columns_40_) output_pixels_ = true;		break;	// 40-column screen start.
				case 315:	if(columns_40_) output_pixels_ = false;		break;	// 40-column screen end.

				case 328:	// Refresh single clock end.
					refresh_ = false;
				break;

				case 336:	// Increment blink, increment refresh end.
				break;

				case 376:	// Increment vertical line.
					++vertical_counter_;
				break;

				case 384:	// Burst start, end of screen — clear vertical line, vertical sub and character reload registers.
					horizontal_burst_ = true;
					// TODO: rest.
				break;

				case 344:	horizontal_blank_ = true;	break;	// Horizontal blanking start.
				case 358:	horizontal_sync_ = true;	break;	// Horizontal sync start.
				case 390:	horizontal_sync_ = false;	break;	// Horizontal sync end.
				case 408:	horizontal_burst_ = false;	break;	// Burst end.
				case 416:	horizontal_blank_ = false;	break;	// Horizontal blanking end.

				case 424:	// Increment character position reload.
				break;

				case 440:	// Video shift register start.
				break;

				case 465:	// Wraparound.
					horizontal_counter_ = 0;
				break;
			}
		}
	}

	void set_scan_target(Outputs::Display::ScanTarget *const target) {
		crt_.set_scan_target(target);
	}

	Outputs::Display::ScanStatus get_scaled_scan_status() const {
		return crt_.get_scaled_scan_status();
	}

private:
	Outputs::CRT::CRT crt_;
	Cycles subcycles_;

	// Programmable values.
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

	// Field position.
	int horizontal_counter_ = 0;
	int vertical_counter_ = 0;

	// Running state.
	bool vertical_blank_ = false;
	bool vertical_sync_ = false;
	bool vertical_window_ = false;
	bool horizontal_blank_ = false;
	bool horizontal_sync_ = false;
	bool horizontal_burst_ = false;

	uint16_t character_address_ = 0;
	uint16_t line_character_address_ = 0;
	bool fetch_characters_;
	bool output_pixels_;
	bool refresh_ = false;

	enum class OutputState {
		Blank,
		Sync,
		Burst,
		Border,
		Pixels,
	} output_state_ = OutputState::Blank;
	int time_in_state_ = 0;

	const Commodore::Plus4::Pager &pager_;
};

}
