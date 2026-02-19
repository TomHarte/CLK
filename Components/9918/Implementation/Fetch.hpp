//
//  Fetch.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 01/01/2023.
//  Copyright © 2023 Thomas Harte. All rights reserved.
//

#pragma once

namespace TI::TMS {

/*
	Fetching routines follow below; they obey the following rules:

		1)	input is a start position and an end position; they should perform the proper
			operations for the period: start <= time < end.
		2)	times are measured relative to the an appropriate clock — they directly
			count access windows on the TMS and Master System, and cycles on a Yamaha.
		3)	within each sequencer, cycle are numbered as per Grauw's timing diagrams. The difference
			between those and internal timing, if there is one, is handled by the dispatcher.
		4)	all of these functions are templated with a `use_end` parameter. That will be true if
			end is < [cycles per line], false otherwise. So functions can use it to eliminate
			should-exit-now checks (which is likely to be the more usual path of execution).

	Provided for the benefit of the methods below:

		*	the function external_slot(), which will perform any pending VRAM read/write.

	All functions should just spool data to intermediary storage. Fetching and drawing are decoupled.
*/

// MARK: - Address mask helpers.

/// @returns An instance of @c AddressT with all top bits set down to and including
/// bit @c end and all others clear.
///
/// So e.g. if @c AddressT is @c uint16_t and this VDP has a 15-bit address space then
/// @c top_bits<10> will be the address with bits 15 to 10 (inclusive) set and the rest clear.
template <typename AddressT, int end> constexpr AddressT top_bits() {
	return AddressT(~0) - AddressT((1 << end) - 1);
}

/// Modifies and returns @c source so that all bits above position @c n are set; the others are unmodified.
template <int n, typename AddressT> constexpr AddressT bits(AddressT source = 0) {
	return AddressT(source | top_bits<AddressT, n>());
}

// MARK: - 171-window Dispatcher.

template <Personality personality>
template<bool use_end, typename SequencerT> void Base<personality>::dispatch(SequencerT &fetcher, int start, int end) {
#define index(n)						\
	if(use_end && end == n) return;		\
	[[fallthrough]];					\
	case n: fetcher.template fetch<from_internal<personality, Clock::FromStartOfSync>(n)>();

	switch(start) {
		default: assert(false);
		index(0);	index(1);	index(2);	index(3);	index(4);
		index(5);	index(6);	index(7);	index(8);	index(9);
		index(10);	index(11);	index(12);	index(13);	index(14);
		index(15);	index(16);	index(17);	index(18);	index(19);
		index(20);	index(21);	index(22);	index(23);	index(24);
		index(25);	index(26);	index(27);	index(28);	index(29);
		index(30);	index(31);	index(32);	index(33);	index(34);
		index(35);	index(36);	index(37);	index(38);	index(39);
		index(40);	index(41);	index(42);	index(43);	index(44);
		index(45);	index(46);	index(47);	index(48);	index(49);
		index(50);	index(51);	index(52);	index(53);	index(54);
		index(55);	index(56);	index(57);	index(58);	index(59);
		index(60);	index(61);	index(62);	index(63);	index(64);
		index(65);	index(66);	index(67);	index(68);	index(69);
		index(70);	index(71);	index(72);	index(73);	index(74);
		index(75);	index(76);	index(77);	index(78);	index(79);
		index(80);	index(81);	index(82);	index(83);	index(84);
		index(85);	index(86);	index(87);	index(88);	index(89);
		index(90);	index(91);	index(92);	index(93);	index(94);
		index(95);	index(96);	index(97);	index(98);	index(99);
		index(100);	index(101);	index(102);	index(103);	index(104);
		index(105);	index(106);	index(107);	index(108);	index(109);
		index(110);	index(111);	index(112);	index(113);	index(114);
		index(115);	index(116);	index(117);	index(118);	index(119);
		index(120);	index(121);	index(122);	index(123);	index(124);
		index(125);	index(126);	index(127);	index(128);	index(129);
		index(130);	index(131);	index(132);	index(133);	index(134);
		index(135);	index(136);	index(137);	index(138);	index(139);
		index(140);	index(141);	index(142);	index(143);	index(144);
		index(145);	index(146);	index(147);	index(148);	index(149);
		index(150);	index(151);	index(152);	index(153);	index(154);
		index(155);	index(156);	index(157);	index(158);	index(159);
		index(160);	index(161);	index(162);	index(163);	index(164);
		index(165);	index(166);	index(167);	index(168);	index(169);
		index(170);
	}

#undef index
}

// MARK: - Fetchers.

template <Personality personality>
struct TextFetcher {
	using AddressT = typename Base<personality>::AddressT;

	TextFetcher(Base<personality> *base, uint8_t y) :
		base(base),
		row_base(base->pattern_name_address_ & bits<10>(AddressT((y >> 3) * 40))),
		row_offset(base->pattern_generator_table_address_ & bits<11>(AddressT(y & 7))) {}

	void fetch_name(AddressT column, int slot = 0) {
		base->name_[slot] = base->ram_[row_base + column];
	}

	void fetch_pattern(AddressT column, int slot = 0) {
		base->fetch_line_buffer_->characters.shapes[column] = base->ram_[row_offset + size_t(base->name_[slot] << 3)];
	}

	Base<personality> *const base;
	const AddressT row_base;
	const AddressT row_offset;
};

template <Personality personality>
struct CharacterFetcher {
	using AddressT = typename Base<personality>::AddressT;

	CharacterFetcher(Base<personality> *const base, const uint8_t y) :
		base(base),
		y(y),
		row_base(base->pattern_name_address_ & bits<10>(AddressT((y << 2)&~31)))
	{
		pattern_base = base->pattern_generator_table_address_;
		colour_base = base->colour_table_address_;
		colour_name_shift = 6;

		const ScreenMode mode = base->fetch_line_buffer_->screen_mode;
		if(mode == ScreenMode::Graphics || mode == ScreenMode::YamahaGraphics3) {
			// If this is high resolution mode, allow the row number to affect the pattern and colour addresses.
			pattern_base &= bits<13>(AddressT(((y & 0xc0) << 5)));
			colour_base &= bits<13>(AddressT(((y & 0xc0) << 5)));

			colour_base += AddressT(y & 7);
			colour_name_shift = 0;
		} else {
			colour_base &= bits<6, AddressT>();
			pattern_base &= bits<11, AddressT>();
		}

		if(mode == ScreenMode::MultiColour) {
			pattern_base += AddressT((y >> 2) & 7);
		} else {
			pattern_base += AddressT(y & 7);
		}
	}

	void fetch_name(const int column) {
		base->tile_offset_ = base->ram_[row_base + AddressT(column)];
	}

	void fetch_pattern(const int column) {
		base->fetch_line_buffer_->tiles.patterns[column][0] = base->ram_[pattern_base + AddressT(base->tile_offset_ << 3)];
	}

	void fetch_colour(const int column) {
		base->fetch_line_buffer_->tiles.patterns[column][1] = base->ram_[colour_base + AddressT((base->tile_offset_ << 3) >> colour_name_shift)];
	}

	Base<personality> *const base;
	const uint8_t y;
	const AddressT row_base;
	AddressT pattern_base;
	AddressT colour_base;
	int colour_name_shift;
};

constexpr SpriteMode sprite_mode(ScreenMode screen_mode) {
	switch(screen_mode) {
		default:
			return SpriteMode::Mode2;

		case ScreenMode::MultiColour:
		case ScreenMode::ColouredText:
		case ScreenMode::Graphics:
			return SpriteMode::Mode1;

		case ScreenMode::SMSMode4:
			return SpriteMode::MasterSystem;
	}
}

// TODO: should this be extended to include Master System sprites?
template <Personality personality, SpriteMode mode>
class SpriteFetcher {
public:
	using AddressT = typename Base<personality>::AddressT;

	// The Yamaha VDP adds an additional table when in Sprite Mode 2, the sprite colour
	// table, which is intended to fill the 512 bytes before the programmer-located sprite
	// attribute table.
	//
	// It partially enforces this proximity by forcing bits 7 and 8 to 0 in the address of
	// the attribute table, and forcing them to 1 but masking out bit 9 for the colour table.
	//
	// AttributeAddressMask is used to enable or disable that behaviour.
	static constexpr AddressT AttributeAddressMask = (mode == SpriteMode::Mode2) ? AddressT(~0x180) : AddressT(~0x000);

	SpriteFetcher(Base<personality> *base, uint8_t y) :
		base(base),
		y(y) {}

	void fetch_location(int slot) {
		fetch_xy(slot);

		if constexpr (mode == SpriteMode::Mode2) {
			fetch_xy(slot + 1);

			base->name_[0] = name(slot);
			base->name_[1] = name(slot + 1);
		}
	}

	void fetch_pattern(int slot) {
		switch(mode) {
			case SpriteMode::Mode1:
				fetch_image(slot, name(slot));
			break;

			case SpriteMode::Mode2:
				fetch_image(slot, base->name_[0]);
				fetch_image(slot + 1, base->name_[1]);
			break;
		}
	}

	void fetch_y(int sprite) {
		const AddressT address = base->sprite_attribute_table_address_ & AttributeAddressMask & bits<7>(AddressT(sprite << 2));
		const uint8_t sprite_y = base->ram_[address];
		base->posit_sprite(sprite, sprite_y, y);
	}

private:
	void fetch_xy(int slot) {
		auto &buffer = *base->fetch_sprite_buffer_;
		buffer.active_sprites[slot].x =
			base->ram_[
				base->sprite_attribute_table_address_ & AttributeAddressMask & bits<7>(AddressT((buffer.active_sprites[slot].index << 2) | 1))
			];
	}

	uint8_t name(int slot) {
		auto &buffer = *base->fetch_sprite_buffer_;
		const AddressT address =
			base->sprite_attribute_table_address_ &
			AttributeAddressMask &
			bits<7>(AddressT((buffer.active_sprites[slot].index << 2) | 2));
		const uint8_t name = base->ram_[address] & (base->sprites_16x16_ ? ~3 : ~0);
		return name;
	}

	void fetch_image(int slot, uint8_t name) {
		uint8_t colour = 0;
		auto &sprite = base->fetch_sprite_buffer_->active_sprites[slot];
		switch(mode) {
			case SpriteMode::Mode1:
				// Fetch colour from the attribute table, per this sprite's slot.
				colour = base->ram_[
					base->sprite_attribute_table_address_ & bits<7>(AddressT((sprite.index << 2) | 3))
				];
			break;

			case SpriteMode::Mode2: {
				// Fetch colour from the colour table, per this sprite's slot and row.
				const AddressT colour_table_address = (base->sprite_attribute_table_address_ | ~AttributeAddressMask) & AddressT(~0x200);
				colour = base->ram_[
					colour_table_address &
					bits<9>(
						AddressT(sprite.index << 4) |
						AddressT(sprite.row)
					)
				];
			} break;
		}
		sprite.image[2] = colour;
		sprite.x -= sprite.early_clock();

		const AddressT graphic_location =
			base->sprite_generator_table_address_ & bits<11>(AddressT((name << 3) | sprite.row));
		sprite.image[0] = base->ram_[graphic_location];
		sprite.image[1] = base->ram_[graphic_location | 16];

		if constexpr (SpriteBuffer::test_is_filling) {
			if(slot == ((mode == SpriteMode::Mode2) ? 7 : 3)) {
				base->fetch_sprite_buffer_->is_filling = false;
			}
		}
	}

	Base<personality> *const base;
	const uint8_t y;
};

template <Personality personality>
struct SMSFetcher {
	using AddressT = typename Base<personality>::AddressT;
	struct RowInfo {
		AddressT pattern_address_base;
		AddressT sub_row[2];
	};

	SMSFetcher(Base<personality> *base, uint8_t y) :
		base(base),
		storage(static_cast<Storage<personality> *>(base)),
		y(y),
		horizontal_offset((y >= 16 || !storage->horizontal_scroll_lock_) ? (base->fetch_line_buffer_->latched_horizontal_scroll >> 3) : 0)
	{
		// Limit address bits in use if this is a SMS2 mode.
		const bool is_tall_mode = base->mode_timing_.pixel_lines != 192;
		const AddressT pattern_name_address = storage->pattern_name_address_ | (is_tall_mode ? 0x800 : 0);
		const AddressT pattern_name_offset = is_tall_mode ? 0x100 : 0;

		// Determine row info for the screen both (i) if vertical scrolling is applied; and (ii) if it isn't.
		// The programmer can opt out of applying vertical scrolling to the right-hand portion of the display.
		const int scrolled_row = (y + storage->latched_vertical_scroll_) % (is_tall_mode ? 256 : 224);
		scrolled_row_info.pattern_address_base = (pattern_name_address & bits<11>(AddressT((scrolled_row & ~7) << 3))) - pattern_name_offset;
		scrolled_row_info.sub_row[0] = AddressT((scrolled_row & 7) << 2);
		scrolled_row_info.sub_row[1] = AddressT(28 ^ ((scrolled_row & 7) << 2));
		if(storage->vertical_scroll_lock_) {
			static_row_info.pattern_address_base = bits<11>(AddressT(pattern_name_address & ((y & ~7) << 3))) - pattern_name_offset;
			static_row_info.sub_row[0] = AddressT((y & 7) << 2);
			static_row_info.sub_row[1] = 28 ^ AddressT((y & 7) << 2);
		} else static_row_info = scrolled_row_info;
	}

	void fetch_sprite(int sprite) {
		auto &sprite_buffer = *base->fetch_sprite_buffer_;
		sprite_buffer.active_sprites[sprite].x =
			base->ram_[
				storage->sprite_attribute_table_address_ & bits<7>((sprite_buffer.active_sprites[sprite].index << 1) | 0)
			] - (storage->shift_sprites_8px_left_ ? 8 : 0);
		const uint8_t name = base->ram_[
				storage->sprite_attribute_table_address_ & bits<7>((sprite_buffer.active_sprites[sprite].index << 1) | 1)
			] & (base->sprites_16x16_ ? ~1 : ~0);

		const AddressT graphic_location =
			storage->sprite_generator_table_address_ &
			bits<13>(AddressT((name << 5) | (sprite_buffer.active_sprites[sprite].row << 2)));
		sprite_buffer.active_sprites[sprite].image[0] = base->ram_[graphic_location];
		sprite_buffer.active_sprites[sprite].image[1] = base->ram_[graphic_location+1];
		sprite_buffer.active_sprites[sprite].image[2] = base->ram_[graphic_location+2];
		sprite_buffer.active_sprites[sprite].image[3] = base->ram_[graphic_location+3];
	}

	void fetch_tile_name(int column) {
		const RowInfo &row_info = column < 24 ? scrolled_row_info : static_row_info;
		const size_t scrolled_column = (column - horizontal_offset) & 0x1f;
		const size_t address = row_info.pattern_address_base + (scrolled_column << 1);
		auto &line_buffer = *base->fetch_line_buffer_;

		line_buffer.tiles.flags[column] = base->ram_[address+1];
		base->tile_offset_ = AddressT(
			(((line_buffer.tiles.flags[column]&1) << 8) | base->ram_[address]) << 5
		) + row_info.sub_row[(line_buffer.tiles.flags[column]&4) >> 2];
	}

	void fetch_tile_pattern(int column) {
		auto &line_buffer = *base->fetch_line_buffer_;
		line_buffer.tiles.patterns[column][0] = base->ram_[base->tile_offset_];
		line_buffer.tiles.patterns[column][1] = base->ram_[base->tile_offset_+1];
		line_buffer.tiles.patterns[column][2] = base->ram_[base->tile_offset_+2];
		line_buffer.tiles.patterns[column][3] = base->ram_[base->tile_offset_+3];
	}

	void posit_sprite(int sprite) {
		base->posit_sprite(sprite, base->ram_[storage->sprite_attribute_table_address_ & bits<8>(AddressT(sprite))], y);
	}

	Base<personality> *const base;
	const Storage<personality> *const storage;
	const uint8_t y;
	const int horizontal_offset;
	RowInfo scrolled_row_info, static_row_info;
};

// MARK: - TMS Sequencers.

template <Personality personality>
struct RefreshSequencer {
	RefreshSequencer(Base<personality> *base) : base(base) {}

	template <int cycle> void fetch() {
		if(cycle < 26 || (cycle & 1) || cycle >= 154) {
			base->do_external_slot(to_internal<personality, Clock::TMSMemoryWindow, Clock::FromStartOfSync>(cycle));
		}
	}

	Base<personality> *const base;
};

template <Personality personality>
struct TextSequencer {
	template <typename... Args> TextSequencer(Args&&... args) : fetcher(std::forward<Args>(args)...) {}

	template <int cycle> void fetch() {
		// The first 30 and the final 4 slots are external.
		if constexpr (cycle < 30 || cycle >= 150) {
			fetcher.base->do_external_slot(to_internal<personality, Clock::TMSMemoryWindow, Clock::FromStartOfSync>(cycle));
			return;
		} else {
			// For the 120 slots in between follow a three-step pattern of:
			static constexpr int offset = cycle - 30;
			static constexpr auto column = AddressT(offset / 3);
			switch(offset % 3) {
				case 0:		// (1) fetch tile name.
					fetcher.fetch_name(column);
				break;
				case 1:		// (2) external slot.
					fetcher.base->do_external_slot(to_internal<personality, Clock::TMSMemoryWindow, Clock::FromStartOfSync>(cycle));
				break;
				case 2:		// (3) fetch tile pattern.
					fetcher.fetch_pattern(column);
				break;
			}
		}
	}

	using AddressT = typename Base<personality>::AddressT;
	TextFetcher<personality> fetcher;
};

template <Personality personality>
struct CharacterSequencer {
	template <typename... Args> CharacterSequencer(Args&&... args) :
		character_fetcher(std::forward<Args>(args)...),
		sprite_fetcher(std::forward<Args>(args)...) {}

	template <int cycle> void fetch() {
		if(cycle < 5) {
			character_fetcher.base->do_external_slot(to_internal<personality, Clock::TMSMemoryWindow, Clock::FromStartOfSync>(cycle));
		}

		if(cycle == 5) {
			// Fetch: n1, c2, pat2a, pat2b, y3, x3, n3, c3, pat3a, pat3b.
			sprite_fetcher.fetch_pattern(2);
			sprite_fetcher.fetch_location(3);
			sprite_fetcher.fetch_pattern(3);
		}

		if(cycle > 14 && cycle < 19) {
			character_fetcher.base->do_external_slot(to_internal<personality, Clock::TMSMemoryWindow, Clock::FromStartOfSync>(cycle));
		}

		// Fetch 8 new sprite Y coordinates, to begin selecting sprites for next line.
		if(cycle == 19) {
			sprite_fetcher.fetch_y(0);	sprite_fetcher.fetch_y(1);	sprite_fetcher.fetch_y(2);	sprite_fetcher.fetch_y(3);
			sprite_fetcher.fetch_y(4);	sprite_fetcher.fetch_y(5);	sprite_fetcher.fetch_y(6);	sprite_fetcher.fetch_y(7);
		}

		// Body of line: tiles themselves, plus some additional potential sprites.
		if(cycle >= 27 && cycle < 155) {
			static constexpr int offset = cycle - 27;
			static constexpr int block = offset >> 2;
			static constexpr int sub_block = offset & 3;
			switch(sub_block) {
				case 0:	character_fetcher.fetch_name(block);	break;
				case 1:
					if(!(block & 3)) {
						character_fetcher.base->do_external_slot(to_internal<personality, Clock::TMSMemoryWindow, Clock::FromStartOfSync>(cycle));
					} else {
						static constexpr int sprite = 8 + ((block >> 2) * 3) + ((block & 3) - 1);
						sprite_fetcher.fetch_y(sprite);
					}
				break;
				case 2:
					character_fetcher.fetch_pattern(block);
					character_fetcher.fetch_colour(block);
				break;
				default: break;
			}
		}

		if(cycle >= 155 && cycle < 157) {
			character_fetcher.base->do_external_slot(to_internal<personality, Clock::TMSMemoryWindow, Clock::FromStartOfSync>(cycle));
		}

		if(cycle == 157) {
			// Fetch: y0, x0, n0, c0, pat0a, pat0b, y1, x1, n1, c1, pat1a, pat1b, y2, x2.
			sprite_fetcher.fetch_location(0);
			sprite_fetcher.fetch_pattern(0);
			sprite_fetcher.fetch_location(1);
			sprite_fetcher.fetch_pattern(1);
			sprite_fetcher.fetch_location(2);
		}
	}

	using AddressT = typename Base<personality>::AddressT;
	CharacterFetcher<personality> character_fetcher;
	SpriteFetcher<personality, SpriteMode::Mode1> sprite_fetcher;
};

// MARK: - TMS fetch routines.

template <Personality personality>
template<bool use_end> void Base<personality>::fetch_tms_refresh(uint8_t, int start, int end) {
	RefreshSequencer sequencer(this);
	dispatch<use_end>(sequencer, start, end);
}

template <Personality personality>
template<bool use_end> void Base<personality>::fetch_tms_text(uint8_t y, int start, int end) {
	TextSequencer<personality> sequencer(this, y);
	dispatch<use_end>(sequencer, start, end);
}

template <Personality personality>
template<bool use_end> void Base<personality>::fetch_tms_character(uint8_t y, int start, int end) {
	CharacterSequencer<personality> sequencer(this, y);
	dispatch<use_end>(sequencer, start, end);
}

// MARK: - Master System

template <Personality personality>
struct SMSSequencer {
	template <typename... Args> SMSSequencer(Args&&... args) : fetcher(std::forward<Args>(args)...) {}

	// Cf. https://www.smspower.org/forums/16485-GenesisMode4VRAMTiming with this implementation pegging
	// window 0 to HSYNC low.
	template <int cycle> void fetch() {
		if(cycle < 3) {
			fetcher.base->do_external_slot(to_internal<personality, Clock::TMSMemoryWindow, Clock::FromStartOfSync>(cycle));
		}

		if(cycle == 3) {
			fetcher.fetch_sprite(4);
			fetcher.fetch_sprite(5);
			fetcher.fetch_sprite(6);
			fetcher.fetch_sprite(7);
		}

		if(cycle == 15 || cycle == 16) {
			fetcher.base->do_external_slot(to_internal<personality, Clock::TMSMemoryWindow, Clock::FromStartOfSync>(cycle));
		}

		if(cycle == 17) {
			fetcher.posit_sprite(0);	fetcher.posit_sprite(1);	fetcher.posit_sprite(2);	fetcher.posit_sprite(3);
			fetcher.posit_sprite(4);	fetcher.posit_sprite(5);	fetcher.posit_sprite(6);	fetcher.posit_sprite(7);
			fetcher.posit_sprite(8);	fetcher.posit_sprite(9);	fetcher.posit_sprite(10);	fetcher.posit_sprite(11);
			fetcher.posit_sprite(12);	fetcher.posit_sprite(13);	fetcher.posit_sprite(14);	fetcher.posit_sprite(15);
		}

		if(cycle >= 25 && cycle < 153) {
			static constexpr int offset = cycle - 25;
			static constexpr int block = offset >> 2;
			static constexpr int sub_block = offset & 3;

			switch(sub_block) {
				default: break;

				case 0:	fetcher.fetch_tile_name(block);		break;
				case 1:
					if(!(block & 3)) {
						fetcher.base->do_external_slot(to_internal<personality, Clock::TMSMemoryWindow, Clock::FromStartOfSync>(cycle));
					} else {
						static constexpr int sprite = (8 + ((block >> 2) * 3) + ((block & 3) - 1)) << 1;
						fetcher.posit_sprite(sprite);
						fetcher.posit_sprite(sprite+1);
					}
				break;
				case 2:	fetcher.fetch_tile_pattern(block);	break;
			}
		}

		if(cycle >= 153 && cycle < 157) {
			fetcher.base->do_external_slot(to_internal<personality, Clock::TMSMemoryWindow, Clock::FromStartOfSync>(cycle));
		}

		if(cycle == 157) {
			fetcher.fetch_sprite(0);
			fetcher.fetch_sprite(1);
			fetcher.fetch_sprite(2);
			fetcher.fetch_sprite(3);
		}

		if(cycle >= 169) {
			fetcher.base->do_external_slot(to_internal<personality, Clock::TMSMemoryWindow, Clock::FromStartOfSync>(cycle));
		}
	}

	using AddressT = typename Base<personality>::AddressT;
	SMSFetcher<personality> fetcher;
};

template <Personality personality>
template<bool use_end> void Base<personality>::fetch_sms([[maybe_unused]] uint8_t y, [[maybe_unused]] int start, [[maybe_unused]] int end) {
	if constexpr (is_sega_vdp(personality)) {
		SMSSequencer<personality> sequencer(this, y);
		dispatch<use_end>(sequencer, start, end);
	}
}

// MARK: - Yamaha

template <Personality personality>
template<ScreenMode mode> void Base<personality>::fetch_yamaha(uint8_t y, int end) {
	CharacterFetcher character_fetcher(this, y);
	TextFetcher text_fetcher(this, y);
	SpriteFetcher<personality, sprite_mode(mode)> sprite_fetcher(this, y);

	using Type = typename Storage<personality>::Event::Type;
	while(Storage<personality>::next_event_->offset < end) {
		switch(Storage<personality>::next_event_->type) {
			case Type::External:
				do_external_slot(Storage<personality>::next_event_->offset);
			break;

			case Type::Name:
				switch(mode) {
					case ScreenMode::Text: {
						const auto column = AddressT(Storage<personality>::next_event_->id << 1);

						text_fetcher.fetch_name(column, 0);
						text_fetcher.fetch_name(column + 1, 1);
					} break;

					case ScreenMode::YamahaText80: {
						const auto column = AddressT(Storage<personality>::next_event_->id << 2);
						const auto start = pattern_name_address_ & bits<12>(AddressT((y >> 3) * 80));

						name_[0] = ram_[start + column + 0];
						name_[1] = ram_[start + column + 1];
						name_[2] = ram_[start + column + 2];
						name_[3] = ram_[start + column + 3];
					} break;

					case ScreenMode::Graphics:
					case ScreenMode::MultiColour:
					case ScreenMode::ColouredText:
						character_fetcher.fetch_name(Storage<personality>::next_event_->id);
					break;

					default: break;
				}
			break;

			case Type::Colour:
				switch(mode) {
					case ScreenMode::YamahaText80: {
						const auto column = AddressT(Storage<personality>::next_event_->id);
						const auto address = colour_table_address_ & bits<9>(AddressT((y >> 3) * 10));
						auto &line_buffer = *fetch_line_buffer_;
						line_buffer.characters.flags[column] = ram_[address + column];
					} break;

					case ScreenMode::Graphics:
					case ScreenMode::MultiColour:
					case ScreenMode::ColouredText:
						character_fetcher.fetch_colour(Storage<personality>::next_event_->id);
					break;

					default: break;
				}
			break;

			case Type::Pattern:
				switch(mode) {
					case ScreenMode::Text: {
						const auto column = AddressT(Storage<personality>::next_event_->id << 1);

						text_fetcher.fetch_pattern(column, 0);
						text_fetcher.fetch_pattern(column + 1, 1);
					} break;

					case ScreenMode::YamahaText80: {
						const auto column = Storage<personality>::next_event_->id << 2;
						const auto start = pattern_generator_table_address_ & bits<11>(AddressT(y & 7));
						auto &line_buffer = *fetch_line_buffer_;

						line_buffer.characters.shapes[column + 0] = ram_[start + AddressT(name_[0] << 3)];
						line_buffer.characters.shapes[column + 1] = ram_[start + AddressT(name_[1] << 3)];
						line_buffer.characters.shapes[column + 2] = ram_[start + AddressT(name_[2] << 3)];
						line_buffer.characters.shapes[column + 3] = ram_[start + AddressT(name_[3] << 3)];
					} break;

					case ScreenMode::Graphics:
					case ScreenMode::MultiColour:
					case ScreenMode::ColouredText:
						character_fetcher.fetch_pattern(Storage<personality>::next_event_->id);
					break;

					case ScreenMode::YamahaGraphics3:
						// As per comment elsewhere; my _guess_ is that G3 is slotted as if it were
						// a bitmap mode, with the three bytes that describe each column fitting into
						// the relevant windows.
						character_fetcher.fetch_name(Storage<personality>::next_event_->id);
						character_fetcher.fetch_colour(Storage<personality>::next_event_->id);
						character_fetcher.fetch_pattern(Storage<personality>::next_event_->id);
					break;

					case ScreenMode::YamahaGraphics4:
					case ScreenMode::YamahaGraphics5: {
						const int column = Storage<personality>::next_event_->id << 2;
						const auto start = bits<15>((y << 7) | column);
						auto &line_buffer = *fetch_line_buffer_;

						line_buffer.bitmap[column + 0] = ram_[pattern_name_address_ & AddressT(start + 0)];
						line_buffer.bitmap[column + 1] = ram_[pattern_name_address_ & AddressT(start + 1)];
						line_buffer.bitmap[column + 2] = ram_[pattern_name_address_ & AddressT(start + 2)];
						line_buffer.bitmap[column + 3] = ram_[pattern_name_address_ & AddressT(start + 3)];
					} break;

					case ScreenMode::YamahaGraphics6:
					case ScreenMode::YamahaGraphics7: {
						const uint8_t *const ram2 = &ram_[65536];
						const int column = Storage<personality>::next_event_->id << 3;
						const auto start = bits<15>((y << 7) | (column >> 1));
						auto &line_buffer = *fetch_line_buffer_;

						// Fetch from alternate banks.
						line_buffer.bitmap[column + 0] = ram_[pattern_name_address_ & AddressT(start + 0) & 0xffff];
						line_buffer.bitmap[column + 1] = ram2[pattern_name_address_ & AddressT(start + 0) & 0xffff];
						line_buffer.bitmap[column + 2] = ram_[pattern_name_address_ & AddressT(start + 1) & 0xffff];
						line_buffer.bitmap[column + 3] = ram2[pattern_name_address_ & AddressT(start + 1) & 0xffff];
						line_buffer.bitmap[column + 4] = ram_[pattern_name_address_ & AddressT(start + 2) & 0xffff];
						line_buffer.bitmap[column + 5] = ram2[pattern_name_address_ & AddressT(start + 2) & 0xffff];
						line_buffer.bitmap[column + 6] = ram_[pattern_name_address_ & AddressT(start + 3) & 0xffff];
						line_buffer.bitmap[column + 7] = ram2[pattern_name_address_ & AddressT(start + 3) & 0xffff];
					} break;

					default: break;
				}
			break;

			case Type::SpriteY:
				switch(mode) {
					case ScreenMode::Blank:
					case ScreenMode::Text:
					case ScreenMode::YamahaText80:
						// Ensure the compiler can discard character_fetcher in these modes.
					break;

					default:
						sprite_fetcher.fetch_y(Storage<personality>::next_event_->id);
					break;
				}
			break;

			case Type::SpriteLocation:
				switch(mode) {
					case ScreenMode::Blank:
					case ScreenMode::Text:
					case ScreenMode::YamahaText80:
						// Ensure the compiler can discard character_fetcher in these modes.
					break;

					default:
						sprite_fetcher.fetch_location(Storage<personality>::next_event_->id);
					break;
				}
			break;

			case Type::SpritePattern:
				switch(mode) {
					case ScreenMode::Blank:
					case ScreenMode::Text:
					case ScreenMode::YamahaText80:
						// Ensure the compiler can discard character_fetcher in these modes.
					break;

					default:
						sprite_fetcher.fetch_pattern(Storage<personality>::next_event_->id);
					break;
				}
			break;

			default: break;
		}

		++Storage<personality>::next_event_;
	}
}


template <Personality personality>
template<bool use_end> void Base<personality>::fetch_yamaha(uint8_t y, int, int end) {
	if constexpr (is_yamaha_vdp(personality)) {
		// Dispatch according to [supported] screen mode.
#define Dispatch(mode)	case mode:	fetch_yamaha<mode>(y, end);	break;
		switch(fetch_line_buffer_->screen_mode) {
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

}
