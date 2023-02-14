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
template<bool use_end, typename Fetcher> void Base<personality>::dispatch(Fetcher &fetcher, int start, int end) {
#define index(n)						\
	if(use_end && end == n) return;		\
	[[fallthrough]];					\
	case n: fetcher.template fetch<n>();

	switch(start) {
		default: assert(false);
		index(0);	index(1);	index(2);	index(3);	index(4);	index(5);	index(6);	index(7);	index(8);	index(9);
		index(10);	index(11);	index(12);	index(13);	index(14);	index(15);	index(16);	index(17);	index(18);	index(19);
		index(20);	index(21);	index(22);	index(23);	index(24);	index(25);	index(26);	index(27);	index(28);	index(29);
		index(30);	index(31);	index(32);	index(33);	index(34);	index(35);	index(36);	index(37);	index(38);	index(39);
		index(40);	index(41);	index(42);	index(43);	index(44);	index(45);	index(46);	index(47);	index(48);	index(49);
		index(50);	index(51);	index(52);	index(53);	index(54);	index(55);	index(56);	index(57);	index(58);	index(59);
		index(60);	index(61);	index(62);	index(63);	index(64);	index(65);	index(66);	index(67);	index(68);	index(69);
		index(70);	index(71);	index(72);	index(73);	index(74);	index(75);	index(76);	index(77);	index(78);	index(79);
		index(80);	index(81);	index(82);	index(83);	index(84);	index(85);	index(86);	index(87);	index(88);	index(89);
		index(90);	index(91);	index(92);	index(93);	index(94);	index(95);	index(96);	index(97);	index(98);	index(99);
		index(100);	index(101);	index(102);	index(103);	index(104);	index(105);	index(106);	index(107);	index(108);	index(109);
		index(110);	index(111);	index(112);	index(113);	index(114);	index(115);	index(116);	index(117);	index(118);	index(119);
		index(120);	index(121);	index(122);	index(123);	index(124);	index(125);	index(126);	index(127);	index(128);	index(129);
		index(130);	index(131);	index(132);	index(133);	index(134);	index(135);	index(136);	index(137);	index(138);	index(139);
		index(140);	index(141);	index(142);	index(143);	index(144);	index(145);	index(146);	index(147);	index(148);	index(149);
		index(150);	index(151);	index(152);	index(153);	index(154);	index(155);	index(156);	index(157);	index(158);	index(159);
		index(160);	index(161);	index(162);	index(163);	index(164);	index(165);	index(166);	index(167);	index(168);	index(169);
		index(170);
	}

#undef index
}

namespace {

template <Personality personality>
struct RefreshFetcher {
	RefreshFetcher(Base<personality> *base) : base(base) {}

	template <int cycle> void fetch() {
		// Do 44 external slots in a block, then treat every other slot as external.
		if(cycle < 44 || (cycle&1)) {
			base->do_external_slot(to_internal<personality, Clock::TMSMemoryWindow>(cycle));
		}
	}

	Base<personality> *const base;
};

template <Personality personality>
struct TextFetcher {
	using AddressT = typename Base<personality>::AddressT;

	TextFetcher(Base<personality> *base, LineBuffer &buffer, int y) :
		base(base),
		line_buffer(buffer),
		row_base(base->pattern_name_address_ & (0x3c00 | size_t(y >> 3) * 40)),
		row_offset(base->pattern_generator_table_address_ & (0x3800 | (y & 7))) {}

	template <int cycle> void fetch() {
		// The first 47 and the final 4 slots are external.
		if constexpr (cycle < 47 || cycle >= 167) {
			base->do_external_slot(to_internal<personality, Clock::TMSMemoryWindow>(cycle));
			return;
		}

		// For the 120 slots in between follow a three-step pattern of:
		if constexpr (cycle >= 47) {
			constexpr int offset = cycle - 47;
			constexpr auto column = AddressT(offset / 3);
			switch(offset % 3) {
				case 0:	base->tile_offset_ = base->ram_[row_base + column];											break;	// (1) fetch tile name.
				case 1:	base->do_external_slot(to_internal<personality, Clock::TMSMemoryWindow>(cycle));			break;	// (2) external slot.
				case 2:	line_buffer.patterns[column][0] = base->ram_[row_offset + size_t(base->tile_offset_ << 3)];	break;	// (3) fetch tile pattern.
			}
		}
	}

	Base<personality> *const base;
	LineBuffer &line_buffer;
	const AddressT row_base;
	const AddressT row_offset;
};

}

template <Personality personality>
template<bool use_end> void Base<personality>::fetch_tms_refresh(LineBuffer &, int, int start, int end) {
	RefreshFetcher refresher(this);
	dispatch<use_end>(refresher, start, end);
}

template <Personality personality>
template<bool use_end> void Base<personality>::fetch_tms_text(LineBuffer &line_buffer, int y, int start, int end) {
	TextFetcher fetcher(this, line_buffer, y);
	dispatch<use_end>(fetcher, start, end);
}

template <Personality personality>
template<bool use_end> void Base<personality>::fetch_tms_character(LineBuffer &line_buffer, int y, int start, int end) {
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
	slot(location): posit_sprite(sprite_selection_buffer, sprite, ram_[sprite_attribute_table_address_ & (((sprite) << 2) | 0x3f80)], y);

#define fetch_tile_name(column) tile_offset_ = ram_[(row_base + column) & 0x3fff];

#define fetch_tile(column)	{\
		line_buffer.patterns[column][1] = ram_[(colour_base + size_t((tile_offset_ << 3) >> colour_name_shift)) & 0x3fff];		\
		line_buffer.patterns[column][0] = ram_[(pattern_base + size_t(tile_offset_ << 3)) & 0x3fff];	\
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

	LineBuffer &sprite_selection_buffer = line_buffers_[(y + 1) % mode_timing_.total_lines];
	const size_t row_base = pattern_name_address_ & (size_t((y << 2)&~31) | 0x3c00);

	size_t pattern_base = pattern_generator_table_address_;
	size_t colour_base = colour_table_address_;
	int colour_name_shift = 6;

	if(screen_mode_ == ScreenMode::Graphics) {
		// If this is high resolution mode, allow the row number to affect the pattern and colour addresses.
		pattern_base &= size_t(0x2000 | ((y & 0xc0) << 5));
		colour_base &= size_t(0x2000 | ((y & 0xc0) << 5));

		colour_base += size_t(y & 7);
		colour_name_shift = 0;
	} else {
		colour_base &= size_t(0xffc0);
		pattern_base &= size_t(0x3800);
	}

	if(screen_mode_ == ScreenMode::MultiColour) {
		pattern_base += size_t((y >> 2) & 7);
	} else {
		pattern_base += size_t(y & 7);
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
template<bool use_end> void Base<personality>::fetch_sms(LineBuffer &line_buffer, int y, int start, int end) {
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
		posit_sprite(sprite_selection_buffer, sprite, ram_[Storage<personality>::sprite_attribute_table_address_ & ((sprite) | 0x3f00)], y);	\
		posit_sprite(sprite_selection_buffer, sprite+1, ram_[Storage<personality>::sprite_attribute_table_address_ & ((sprite + 1) | 0x3f00)], y);	\

#define fetch_tile_name(column, row_info)	{\
		const size_t scrolled_column = (column - horizontal_offset) & 0x1f;\
		const size_t address = row_info.pattern_address_base + (scrolled_column << 1);	\
		line_buffer.flags[column] = ram_[address+1];	\
		tile_offset_ = size_t(	\
			(((line_buffer.flags[column]&1) << 8) | ram_[address]) << 5	\
		) + row_info.sub_row[(line_buffer.flags[column]&4) >> 2];	\
	}

#define fetch_tile(column)	\
	line_buffer.patterns[column][0] = ram_[tile_offset_];	\
	line_buffer.patterns[column][1] = ram_[tile_offset_+1];	\
	line_buffer.patterns[column][2] = ram_[tile_offset_+2];	\
	line_buffer.patterns[column][3] = ram_[tile_offset_+3];

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
	LineBuffer &sprite_selection_buffer = line_buffers_[(y + 1) % mode_timing_.total_lines];
	const int horizontal_offset = (y >= 16 || !Storage<personality>::horizontal_scroll_lock_) ? (line_buffer.latched_horizontal_scroll >> 3) : 0;

	// Limit address bits in use if this is a SMS2 mode.
	const bool is_tall_mode = mode_timing_.pixel_lines != 192;
	const size_t pattern_name_address = Storage<personality>::pattern_name_address_ | (is_tall_mode ? 0x800 : 0);
	const size_t pattern_name_offset = is_tall_mode ? 0x100 : 0;

	// Determine row info for the screen both (i) if vertical scrolling is applied; and (ii) if it isn't.
	// The programmer can opt out of applying vertical scrolling to the right-hand portion of the display.
	const int scrolled_row = (y + Storage<personality>::latched_vertical_scroll_) % (is_tall_mode ? 256 : 224);
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
		row_info.pattern_address_base = (pattern_name_address & size_t(((y & ~7) << 3) | 0x3800)) - pattern_name_offset;
		row_info.sub_row[0] = size_t((y & 7) << 2);
		row_info.sub_row[1] = 28 ^ size_t((y & 7) << 2);
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

template <Personality personality>
template<ScreenMode mode> void Base<personality>::fetch_yamaha([[maybe_unused]] LineBuffer &line_buffer, [[maybe_unused]] int y, int end) {
	const AddressT rotated_name_ = pattern_name_address_ >> 1;
	const uint8_t *const ram2 = &ram_[65536];

	using Type = typename Storage<personality>::Event::Type;
	while(Storage<personality>::next_event_->offset < end) {
		switch(Storage<personality>::next_event_->type) {
			case Type::External:
				do_external_slot(Storage<personality>::next_event_->offset);
			break;

			case Type::Name:
				switch(mode) {
					case ScreenMode::Text:
						// TODO: read two new character names.
					break;

					case ScreenMode::YamahaText80:
						// TODO: read four new character names.
					break;

					default: break;
				}
			break;

			case Type::Colour:
				switch(mode) {
					case ScreenMode::YamahaText80:
						// TODO: read a single 'colour' (i.e. a bitfield, governing colour selection for eight characters).
					break;

					default: break;
				}
			break;

			case Type::Pattern:
				switch(mode) {
					case ScreenMode::Text:
						// TODO: look up two sets of character contents, based on names from earlier.
					break;

					case ScreenMode::YamahaText80:
						// TODO: look up four sets of character contents, based on names from earlier.
					break;

					default: break;
				}
			break;

			case Type::DataBlock:
				// Exactly how to fetch depends upon mode...
				switch(mode) {
					case ScreenMode::YamahaGraphics4:
					case ScreenMode::YamahaGraphics5: {
						const int column = Storage<personality>::data_block_;
						Storage<personality>::data_block_ += 4;

						const int start = (y << 7) | column | 0x1'8000;

						line_buffer.bitmap[column + 0] = ram_[pattern_name_address_ & AddressT(start + 0)];
						line_buffer.bitmap[column + 1] = ram_[pattern_name_address_ & AddressT(start + 1)];
						line_buffer.bitmap[column + 2] = ram_[pattern_name_address_ & AddressT(start + 2)];
						line_buffer.bitmap[column + 3] = ram_[pattern_name_address_ & AddressT(start + 3)];
					} break;

					case ScreenMode::YamahaGraphics6:
					case ScreenMode::YamahaGraphics7: {
						const int column = Storage<personality>::data_block_ << 1;
						Storage<personality>::data_block_ += 4;

						const int start = (y << 7) | column | 0x1'8000;

						// Fetch from alternate banks.
						line_buffer.bitmap[column + 0] = ram_[rotated_name_ & AddressT(start + 0)];
						line_buffer.bitmap[column + 1] = ram2[rotated_name_ & AddressT(start + 0)];
						line_buffer.bitmap[column + 2] = ram_[rotated_name_ & AddressT(start + 1)];
						line_buffer.bitmap[column + 3] = ram2[rotated_name_ & AddressT(start + 1)];
						line_buffer.bitmap[column + 4] = ram_[rotated_name_ & AddressT(start + 2)];
						line_buffer.bitmap[column + 5] = ram2[rotated_name_ & AddressT(start + 2)];
						line_buffer.bitmap[column + 6] = ram_[rotated_name_ & AddressT(start + 3)];
						line_buffer.bitmap[column + 7] = ram2[rotated_name_ & AddressT(start + 3)];
					} break;

					default: break;
				}
			break;

			default: break;
		}

		++Storage<personality>::next_event_;
	}
}


template <Personality personality>
template<bool use_end> void Base<personality>::fetch_yamaha(LineBuffer &line_buffer, int y, int, int end) {
	if constexpr (is_yamaha_vdp(personality)) {
		// Dispatch according to [supported] screen mode.
#define Dispatch(mode)	case mode:	fetch_yamaha<mode>(line_buffer, y, end);	break;
		switch(line_buffer.screen_mode) {
			default: break;
			Dispatch(ScreenMode::Blank);
			Dispatch(ScreenMode::Text);
			Dispatch(ScreenMode::MultiColour);
			Dispatch(ScreenMode::ColouredText);
			Dispatch(ScreenMode::Graphics);
			Dispatch(ScreenMode::YamahaText80);
			Dispatch(ScreenMode::YamahaGraphics3);
			Dispatch(ScreenMode::YamahaGraphics4);
			Dispatch(ScreenMode::YamahaGraphics5);
			Dispatch(ScreenMode::YamahaGraphics6);
			Dispatch(ScreenMode::YamahaGraphics7);
		}
#undef Dispatch
	}
}

// MARK: - Mega Drive

// TODO.

#undef external_slot
#undef slot

#endif /* Fetch_hpp */
