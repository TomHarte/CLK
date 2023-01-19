//
//  Fetch.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 01/01/2023.
//  Copyright © 2023 Thomas Harte. All rights reserved.
//

#ifndef Fetch_hpp
#define Fetch_hpp

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
	if(use_end && end == n) return;	\
	[[fallthrough]];				\
	case n

#define external_slot(n)	\
	slot(n): do_external_slot(to_internal<personality, Clock::TMSMemoryWindow>(n));

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


// MARK: - TMS9918

template <Personality personality>
template<bool use_end> void Base<personality>::fetch_tms_refresh(int start, int end) {
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

template <Personality personality>
template<bool use_end> void Base<personality>::fetch_tms_text(int start, int end) {
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

template <Personality personality>
template<bool use_end> void Base<personality>::fetch_tms_character(int start, int end) {
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
			do_external_slot(to_internal<personality, Clock::TMSMemoryWindow>(31));
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


// MARK: - Master System

template <Personality personality>
template<bool use_end> void Base<personality>::fetch_sms(int start, int end) {
	if constexpr (is_sega_vdp(personality)) {

#define sprite_fetch(sprite)	{\
		line_buffer.active_sprites[sprite].x = \
			ram_[\
				Storage<personality>::sprite_attribute_table_address_ & size_t(0x3f80 | (line_buffer.active_sprites[sprite].index << 1))\
			] - (Storage<personality>::shift_sprites_8px_left_ ? 8 : 0);	\
		const uint8_t name = ram_[\
				Storage<personality>::sprite_attribute_table_address_ & size_t(0x3f81 | (line_buffer.active_sprites[sprite].index << 1))\
			] & (sprites_16x16_ ? ~1 : ~0);\
		const size_t graphic_location = Storage<personality>::sprite_generator_table_address_ & size_t(0x2000 | (name << 5) | (line_buffer.active_sprites[sprite].row << 2));	\
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
		posit_sprite(sprite_selection_buffer, sprite, ram_[Storage<personality>::sprite_attribute_table_address_ & ((sprite) | 0x3f00)], write_pointer_.row);	\
		posit_sprite(sprite_selection_buffer, sprite+1, ram_[Storage<personality>::sprite_attribute_table_address_ & ((sprite + 1) | 0x3f00)], write_pointer_.row);	\

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
		fetch_tile_name(column+1, row_info)	\
		sprite_y_read(location+5, sprite);	\
	slot(location+6):	\
	slot(location+7):	\
	slot(location+8):	\
		fetch_tile(column+1)					\
		fetch_tile_name(column+2, row_info)		\
		sprite_y_read(location+9, sprite+2);	\
	slot(location+10):	\
	slot(location+11):	\
	slot(location+12):	\
		fetch_tile(column+2)					\
		fetch_tile_name(column+3, row_info)		\
		sprite_y_read(location+13, sprite+4);	\
	slot(location+14):	\
	slot(location+15): fetch_tile(column+3)

	// Determine the coarse horizontal scrolling offset; this isn't applied on the first two lines if the programmer has requested it.
	LineBuffer &line_buffer = line_buffers_[write_pointer_.row];
	LineBuffer &sprite_selection_buffer = line_buffers_[(write_pointer_.row + 1) % mode_timing_.total_lines];
	const int horizontal_offset = (write_pointer_.row >= 16 || !Storage<personality>::horizontal_scroll_lock_) ? (line_buffer.latched_horizontal_scroll >> 3) : 0;

	// Limit address bits in use if this is a SMS2 mode.
	const bool is_tall_mode = mode_timing_.pixel_lines != 192;
	const size_t pattern_name_address = Storage<personality>::pattern_name_address_ | (is_tall_mode ? 0x800 : 0);
	const size_t pattern_name_offset = is_tall_mode ? 0x100 : 0;

	// Determine row info for the screen both (i) if vertical scrolling is applied; and (ii) if it isn't.
	// The programmer can opt out of applying vertical scrolling to the right-hand portion of the display.
	const int scrolled_row = (write_pointer_.row + Storage<personality>::latched_vertical_scroll_) % (is_tall_mode ? 256 : 224);
	struct RowInfo {
		size_t pattern_address_base;
		size_t sub_row[2];
	};
	const RowInfo scrolled_row_info = {
		(pattern_name_address & size_t(((scrolled_row & ~7) << 3) | 0x3800)) - pattern_name_offset,
		{size_t((scrolled_row & 7) << 2), 28 ^ size_t((scrolled_row & 7) << 2)}
	};
	RowInfo row_info;
	if(Storage<personality>::vertical_scroll_lock_) {
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
			do_external_slot(to_internal<personality, Clock::TMSMemoryWindow>(29));
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
}

// MARK: - Yamaha

// TODO.

template <Personality personality>
template<bool use_end> void Base<personality>::fetch_yamaha_refresh(int start, int end) {
	(void)start;
	(void)end;
}

template <Personality personality>
template<bool use_end> void Base<personality>::fetch_yamaha_no_sprites(int start, int end) {
	(void)start;
	(void)end;
}

template <Personality personality>
template<bool use_end> void Base<personality>::fetch_yamaha_sprites(int start, int end) {
	(void)start;
	(void)end;
}

// MARK: - Mega Drive

// TODO.

#undef external_slot
#undef slot

#endif /* Fetch_hpp */
