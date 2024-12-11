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
};

}
