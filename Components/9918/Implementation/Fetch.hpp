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

// MARK: - TMS fetcher definitions.

template <Personality personality>
struct RefreshFetcher {
	RefreshFetcher(Base<personality> *base) : base(base) {}

	template <int cycle> void fetch() {
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
		} else {
			// For the 120 slots in between follow a three-step pattern of:
			constexpr int offset = cycle - 47;
			constexpr auto column = AddressT(offset / 3);
			switch(offset % 3) {
				case 0:	base->name_[0] = base->ram_[row_base + column];													break;	// (1) fetch tile name.
				case 1:	base->do_external_slot(to_internal<personality, Clock::TMSMemoryWindow>(cycle));				break;	// (2) external slot.
				case 2:	line_buffer.characters.shapes[column] = base->ram_[row_offset + size_t(base->name_[0] << 3)];	break;	// (3) fetch tile pattern.
			}
		}
	}

	Base<personality> *const base;
	LineBuffer &line_buffer;
	const AddressT row_base;
	const AddressT row_offset;
};

template <Personality personality>
struct CharacterFetcher {
	using AddressT = typename Base<personality>::AddressT;

	CharacterFetcher(Base<personality> *base, LineBuffer &buffer, LineBuffer &sprite_selection_buffer, int y) :
		base(base),
		tile_buffer(buffer),
		sprite_selection_buffer(sprite_selection_buffer),
		y(y),
		row_base(base->pattern_name_address_ & (AddressT((y << 2)&~31) | 0x3c00))
	{
		pattern_base = base->pattern_generator_table_address_;
		colour_base = base->colour_table_address_;
		colour_name_shift = 6;

		if(buffer.screen_mode == ScreenMode::Graphics) {
			// If this is high resolution mode, allow the row number to affect the pattern and colour addresses.
			pattern_base &= size_t(0x2000 | ((y & 0xc0) << 5));
			colour_base &= size_t(0x2000 | ((y & 0xc0) << 5));

			colour_base += size_t(y & 7);
			colour_name_shift = 0;
		} else {
			colour_base &= size_t(0xffc0);
			pattern_base &= size_t(0x3800);
		}

		if(buffer.screen_mode == ScreenMode::MultiColour) {
			pattern_base += size_t((y >> 2) & 7);
		} else {
			pattern_base += size_t(y & 7);
		}
	}

	template <int cycle> void fetch() {
		if(cycle < 2) {
			base->do_external_slot(to_internal<personality, Clock::TMSMemoryWindow>(cycle));
		}

		if(cycle == 2) {
			// Fetch: y0, x0, n0, c0, pat0a, pat0b, y1, x1, n1, c1, pat1a, pat1b, y2, x2.
			fetch_sprite_coordinates(0);
			fetch_sprite_graphics(0);
			fetch_sprite_coordinates(1);
			fetch_sprite_graphics(1);
			fetch_sprite_coordinates(2);
		}

		if(cycle > 16 && cycle < 21) {
			base->do_external_slot(to_internal<personality, Clock::TMSMemoryWindow>(cycle));
		}

		if(cycle == 21) {
			// Fetch: n1, c2, pat2a, pat2b, y3, x3, n3, c3, pat3a, pat3b.
			fetch_sprite_graphics(2);
			fetch_sprite_coordinates(3);
			fetch_sprite_graphics(3);

			// All patterns now fetched, reset sprite selection.
			sprite_selection_buffer.reset_sprite_collection();
		}

		if(cycle >= 31 && cycle < 35) {
			base->do_external_slot(to_internal<personality, Clock::TMSMemoryWindow>(cycle));
		}

		// Cycles 35 to 43: fetch 8 new sprite Y coordinates, to begin selecting sprites for next line.
		if(cycle == 35) {
			posit_sprite(0);	posit_sprite(1);	posit_sprite(2);	posit_sprite(3);
			posit_sprite(4);	posit_sprite(5);	posit_sprite(6);	posit_sprite(7);
		}

		// Rest of line: tiles themselves, plus some additional potential sprites.
		if(cycle >= 43) {
			constexpr int offset = cycle - 43;
			constexpr int block = offset >> 2;
			constexpr int sub_block = offset & 3;
			switch(sub_block) {
				case 0:	base->tile_offset_ = base->ram_[(row_base + AddressT(block)) & 0x3fff];	break;
				case 1:
					if(!(block & 3)) {
						base->do_external_slot(to_internal<personality, Clock::TMSMemoryWindow>(cycle));
					} else {
						constexpr int sprite = 8 + ((block >> 2) * 3) + ((block & 3) - 1);
						posit_sprite(sprite);
					}
				break;
				case 3:
					tile_buffer.tiles.patterns[block][0] = base->ram_[(pattern_base + AddressT(base->tile_offset_ << 3)) & 0x3fff];
					tile_buffer.tiles.patterns[block][1] = base->ram_[(colour_base + AddressT((base->tile_offset_ << 3) >> colour_name_shift)) & 0x3fff];
				break;
				default: break;
			}
		}
	}

	void fetch_sprite_coordinates(int sprite) {
		tile_buffer.active_sprites[sprite].x =
			base->ram_[
				base->sprite_attribute_table_address_ & AddressT(0x3f81 | (tile_buffer.active_sprites[sprite].index << 2))
			];
	}

	void fetch_sprite_graphics(int sprite) {
		const uint8_t name = base->ram_[
				base->sprite_attribute_table_address_ & AddressT(0x3f82 | (tile_buffer.active_sprites[sprite].index << 2))
			] & (base->sprites_16x16_ ? ~3 : ~0);
		tile_buffer.active_sprites[sprite].image[2] = base->ram_[
				base->sprite_attribute_table_address_ & AddressT(0x3f83 | (tile_buffer.active_sprites[sprite].index << 2))
			];
		tile_buffer.active_sprites[sprite].x -= (tile_buffer.active_sprites[sprite].image[2] & 0x80) >> 2;

		const size_t graphic_location = base->sprite_generator_table_address_ & size_t(0x3800 | (name << 3) | tile_buffer.active_sprites[sprite].row);
		tile_buffer.active_sprites[sprite].image[0] = base->ram_[graphic_location];
		tile_buffer.active_sprites[sprite].image[1] = base->ram_[graphic_location+16];
	}

	void posit_sprite(int sprite) {
		base->posit_sprite(sprite_selection_buffer, sprite, base->ram_[base->sprite_attribute_table_address_ & AddressT((sprite << 2) | 0x3f80)], y);	
	}

	Base<personality> *const base;
	LineBuffer &tile_buffer;
	LineBuffer &sprite_selection_buffer;
	const int y;
	const AddressT row_base;
	AddressT pattern_base;
	AddressT colour_base;
	int colour_name_shift;
};

// MARK: - TMS fetch routines.

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
	CharacterFetcher fetcher(this, line_buffer, line_buffers_[(y + 1) % mode_timing_.total_lines], y);
	dispatch<use_end>(fetcher, start, end);
}

// MARK: - Master System

template <Personality personality>
struct SMSFetcher {
	using AddressT = typename Base<personality>::AddressT;
	struct RowInfo {
		AddressT pattern_address_base;
		AddressT sub_row[2];
	};

	SMSFetcher(Base<personality> *base, LineBuffer &buffer, LineBuffer &sprite_selection_buffer, int y) :
		base(base),
		storage(static_cast<Storage<personality> *>(base)),
		tile_buffer(buffer),
		sprite_selection_buffer(sprite_selection_buffer),
		y(y),
		horizontal_offset((y >= 16 || !storage->horizontal_scroll_lock_) ? (tile_buffer.latched_horizontal_scroll >> 3) : 0)
	{
		// Limit address bits in use if this is a SMS2 mode.
		const bool is_tall_mode = base->mode_timing_.pixel_lines != 192;
		const AddressT pattern_name_address = storage->pattern_name_address_ | (is_tall_mode ? 0x800 : 0);
		const AddressT pattern_name_offset = is_tall_mode ? 0x100 : 0;

		// Determine row info for the screen both (i) if vertical scrolling is applied; and (ii) if it isn't.
		// The programmer can opt out of applying vertical scrolling to the right-hand portion of the display.
		const int scrolled_row = (y + storage->latched_vertical_scroll_) % (is_tall_mode ? 256 : 224);
		scrolled_row_info.pattern_address_base = AddressT((pattern_name_address & size_t(((scrolled_row & ~7) << 3) | 0x3800)) - pattern_name_offset);
		scrolled_row_info.sub_row[0] = AddressT((scrolled_row & 7) << 2);
		scrolled_row_info.sub_row[1] = AddressT(28 ^ ((scrolled_row & 7) << 2));
		if(storage->vertical_scroll_lock_) {
			static_row_info.pattern_address_base = AddressT((pattern_name_address & AddressT(((y & ~7) << 3) | 0x3800)) - pattern_name_offset);
			static_row_info.sub_row[0] = AddressT((y & 7) << 2);
			static_row_info.sub_row[1] = 28 ^ AddressT((y & 7) << 2);
		} else static_row_info = scrolled_row_info;
	}

	template <int cycle> void fetch() {
		if(!cycle) {
			fetch_sprite(0);
			fetch_sprite(1);
			fetch_sprite(2);
			fetch_sprite(3);
		}

		if(cycle >= 12 && cycle < 17) {
			base->do_external_slot(to_internal<personality, Clock::TMSMemoryWindow>(cycle));
		}

		if(cycle == 17) {
			fetch_sprite(4);
			fetch_sprite(5);
			fetch_sprite(6);
			fetch_sprite(7);
			sprite_selection_buffer.reset_sprite_collection();
		}

		if(cycle == 29 || cycle == 30) {
			base->do_external_slot(to_internal<personality, Clock::TMSMemoryWindow>(cycle));
		}

		if(cycle == 31) {
			posit_sprite(0);	posit_sprite(1);	posit_sprite(2);	posit_sprite(3);
			posit_sprite(4);	posit_sprite(5);	posit_sprite(6);	posit_sprite(7);
			posit_sprite(8);	posit_sprite(9);	posit_sprite(10);	posit_sprite(11);
			posit_sprite(12);	posit_sprite(13);	posit_sprite(14);	posit_sprite(15);
		}

		if(cycle >= 39 && cycle < 167) {
			constexpr int offset = cycle - 39;
			constexpr int block = offset >> 2;
			constexpr int sub_block = offset & 3;

			switch(sub_block) {
				default: break;

				case 0:	fetch_tile_name(block, block < 24 ? scrolled_row_info : static_row_info);	break;
				case 1:
					if(!(block & 3)) {
						base->do_external_slot(to_internal<personality, Clock::TMSMemoryWindow>(cycle));
					} else {
						constexpr int sprite = (8 + ((block >> 2) * 3) + ((block & 3) - 1)) << 1;
						posit_sprite(sprite);
						posit_sprite(sprite+1);
					}
				break;
				case 2:
					tile_buffer.tiles.patterns[block][0] = base->ram_[base->tile_offset_];
					tile_buffer.tiles.patterns[block][1] = base->ram_[base->tile_offset_+1];
					tile_buffer.tiles.patterns[block][2] = base->ram_[base->tile_offset_+2];
					tile_buffer.tiles.patterns[block][3] = base->ram_[base->tile_offset_+3];
				break;
			}
		}

		if(cycle >= 167) {
			base->do_external_slot(to_internal<personality, Clock::TMSMemoryWindow>(cycle));
		}
	}

	void fetch_sprite(int sprite) {
		tile_buffer.active_sprites[sprite].x =
			base->ram_[
				storage->sprite_attribute_table_address_ & size_t(0x3f80 | (tile_buffer.active_sprites[sprite].index << 1))
			] - (storage->shift_sprites_8px_left_ ? 8 : 0);
		const uint8_t name = base->ram_[
				storage->sprite_attribute_table_address_ & size_t(0x3f81 | (tile_buffer.active_sprites[sprite].index << 1))
			] & (base->sprites_16x16_ ? ~1 : ~0);

		const AddressT graphic_location =
			storage->sprite_generator_table_address_ &
			AddressT(0x2000 | (name << 5) | (tile_buffer.active_sprites[sprite].row << 2));
		tile_buffer.active_sprites[sprite].image[0] = base->ram_[graphic_location];
		tile_buffer.active_sprites[sprite].image[1] = base->ram_[graphic_location+1];
		tile_buffer.active_sprites[sprite].image[2] = base->ram_[graphic_location+2];
		tile_buffer.active_sprites[sprite].image[3] = base->ram_[graphic_location+3];
	}

	void fetch_tile_name(int column, const RowInfo &row_info) {
		const size_t scrolled_column = (column - horizontal_offset) & 0x1f;
		const size_t address = row_info.pattern_address_base + (scrolled_column << 1);
		tile_buffer.tiles.flags[column] = base->ram_[address+1];
		base->tile_offset_ = AddressT(
			(((tile_buffer.tiles.flags[column]&1) << 8) | base->ram_[address]) << 5
		) + row_info.sub_row[(tile_buffer.tiles.flags[column]&4) >> 2];
	}

	void posit_sprite(int sprite) {
		base->posit_sprite(sprite_selection_buffer, sprite, base->ram_[storage->sprite_attribute_table_address_ & (sprite | 0x3f00)], y);
	}

	Base<personality> *const base;
	const Storage<personality> *const storage;
	LineBuffer &tile_buffer;
	LineBuffer &sprite_selection_buffer;
	const int y;
	const int horizontal_offset;
	RowInfo scrolled_row_info, static_row_info;
};

template <Personality personality>
template<bool use_end> void Base<personality>::fetch_sms(LineBuffer &line_buffer, int y, int start, int end) {
	if constexpr (is_sega_vdp(personality)) {
		SMSFetcher fetcher(this, line_buffer, line_buffers_[(y + 1) % mode_timing_.total_lines], y);
		dispatch<use_end>(fetcher, start, end);
	}
}

// MARK: - Yamaha

template <Personality personality>
template<ScreenMode mode> void Base<personality>::fetch_yamaha(LineBuffer &line_buffer, int y, int end) {
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
					case ScreenMode::Text: {
						const auto column = AddressT(Storage<personality>::data_block_);
						const AddressT start = pattern_name_address_ & (0x1fc00 | size_t(y >> 3) * 40);

						name_[0] = ram_[start + column + 0];
						name_[1] = ram_[start + column + 1];
					} break;

					case ScreenMode::YamahaText80: {
						const auto column = AddressT(Storage<personality>::data_block_);
						const AddressT start = pattern_name_address_ & (0x1f000 | size_t(y >> 3) * 80);

						name_[0] = ram_[start + column + 0];
						name_[1] = ram_[start + column + 1];
						name_[2] = ram_[start + column + 2];
						name_[3] = ram_[start + column + 3];
					} break;

					default: break;
				}
			break;

			case Type::Colour:
				switch(mode) {
					case ScreenMode::YamahaText80: {
						const auto column = AddressT(Storage<personality>::data_block_ >> 3);
						const AddressT address = colour_table_address_ & (0x1fe00 | size_t(y >> 3) * 10);
						line_buffer.characters.flags[column] = ram_[address + column];
					} break;

					default: break;
				}
			break;

			case Type::Pattern:
				switch(mode) {
					case ScreenMode::Text: {
						const auto column = AddressT(Storage<personality>::data_block_);
						Storage<personality>::data_block_ += 2;

						const AddressT start = pattern_generator_table_address_ & (0x1f800 | (y & 7));
						line_buffer.characters.shapes[column + 0] = ram_[start + AddressT(name_[0] << 3)];
						line_buffer.characters.shapes[column + 1] = ram_[start + AddressT(name_[1] << 3)];
					} break;

					case ScreenMode::YamahaText80: {
						const auto column = AddressT(Storage<personality>::data_block_);
						Storage<personality>::data_block_ += 4;

						const AddressT start = pattern_generator_table_address_ & (0x1f800 | (y & 7));
						line_buffer.characters.shapes[column + 0] = ram_[start + AddressT(name_[0] << 3)];
						line_buffer.characters.shapes[column + 1] = ram_[start + AddressT(name_[1] << 3)];
						line_buffer.characters.shapes[column + 2] = ram_[start + AddressT(name_[2] << 3)];
						line_buffer.characters.shapes[column + 3] = ram_[start + AddressT(name_[3] << 3)];
					} break;

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

#endif /* Fetch_hpp */
