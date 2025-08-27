//
//  Draw.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 05/01/2023.
//  Copyright © 2023 Thomas Harte. All rights reserved.
//

#pragma once

namespace TI::TMS {

// MARK: - Sprites, as generalised.

template <Personality personality>
template <SpriteMode mode, bool double_width>
void Base<personality>::draw_sprites(
	[[maybe_unused]] const uint8_t y,
	const int start,
	const int end,
	const std::array<uint32_t, 16> &palette,
	int *const colour_buffer
) {
	if(!draw_line_buffer_->sprites) {
		return;
	}

	auto &buffer = *draw_line_buffer_->sprites;
	if(!buffer.active_sprite_slot) {
		return;
	}

	const int shift_advance = sprites_magnified_ ? 1 : 2;

	// If this is the start of the line clip any part of any sprites that is off to the left.
	if(!start) {
		for(int index = 0; index < buffer.active_sprite_slot; ++index) {
			auto &sprite = buffer.active_sprites[index];
			if(sprite.x < 0) sprite.shift_position -= shift_advance * sprite.x;
		}
	}

	int sprite_buffer[256];
	int sprite_collision = 0;
	memset(&sprite_buffer[start], 0, size_t(end - start)*sizeof(sprite_buffer[0]));

	if constexpr (mode == SpriteMode::MasterSystem) {
		// Draw all sprites into the sprite buffer.
		for(int index = buffer.active_sprite_slot - 1; index >= 0; --index) {
			auto &sprite = buffer.active_sprites[index];
			if(sprite.shift_position >= 16) {
				continue;
			}

			const int pixel_start = std::max(start, sprite.x);

			// TODO: it feels like the work below should be simplifiable;
			// the double shift in particular, and hopefully the variable shift.
			for(int c = pixel_start; c < end && sprite.shift_position < 16; ++c) {
				const int shift = (sprite.shift_position >> 1);
				const int sprite_colour =
					(((sprite.image[3] << shift) & 0x80) >> 4) |
					(((sprite.image[2] << shift) & 0x80) >> 5) |
					(((sprite.image[1] << shift) & 0x80) >> 6) |
					(((sprite.image[0] << shift) & 0x80) >> 7);

				if(sprite_colour) {
					sprite_collision |= sprite_buffer[c];
					sprite_buffer[c] = sprite_colour | 0x10;
				}

				sprite.shift_position += shift_advance;
			}
		}

		// Draw the sprite buffer onto the colour buffer, wherever the tile map doesn't have
		// priority (or is transparent).
		for(int c = start; c < end; ++c) {
			if(
				sprite_buffer[c] &&
				(!(colour_buffer[c]&0x20) || !(colour_buffer[c]&0xf))
			) colour_buffer[c] = sprite_buffer[c];
		}

		if(sprite_collision) {
			status_ |= StatusSpriteCollision;
		}

		return;
	}

	if constexpr (SpriteBuffer::test_is_filling) {
		assert(!buffer.is_filling);
	}

	static constexpr uint32_t sprite_colour_selection_masks[2] = {0x00000000, 0xffffffff};
	static constexpr int colour_masks[16] = {0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1};
	const int sprite_width = sprites_16x16_ ? 16 : 8;
	const int shifter_target = sprite_width << 1;
	const int pixel_width = sprites_magnified_ ? sprite_width << 1 : sprite_width;
	int min_sprite = 0;

	//
	// Approach taken for Mode 2 sprites:
	//
	//	(1)	precompute full sprite images, at up to 32 pixels wide;
	//	(2) for each sprite that is marked as CC, walk backwards until the
	//		first sprite that is not marked CC, ORing it into the precomputed
	//		image at each step;
	//	(3)	subsequently, just draw each sprite image independently.
	//
	if constexpr (mode == SpriteMode::Mode2) {
		// Determine the lowest visible sprite; exit early if that leaves no sprites visible.
		for(; min_sprite < buffer.active_sprite_slot; min_sprite++) {
			auto &sprite = buffer.active_sprites[min_sprite];
			if(sprite.opaque()) {
				break;
			}
		}
		if(min_sprite == buffer.active_sprite_slot) {
			return;
		}

		if(!start) {
			// Pre-rasterise the sprites one-by-one.
			if(sprites_magnified_) {
				for(int index = min_sprite; index < buffer.active_sprite_slot; index++) {
					auto &sprite = buffer.active_sprites[index];
					for(int c = 0; c < 32; c+= 2) {
						const int shift = (c >> 1) ^ 7;
						const int bit = 1 & (sprite.image[shift >> 3] >> (shift & 7));

						Storage<personality>::sprite_cache_[index][c] =
						Storage<personality>::sprite_cache_[index][c + 1] =
							(sprite.image[2] & 0xf & sprite_colour_selection_masks[bit]) |
							uint8_t((bit << StatusSpriteCollisionShift) & sprite.collision_bit());
					}
				}
			} else {
				for(int index = min_sprite; index < buffer.active_sprite_slot; index++) {
					auto &sprite = buffer.active_sprites[index];
					for(int c = 0; c < 16; c++) {
						const int shift = c ^ 7;
						const int bit = 1 & (sprite.image[shift >> 3] >> (shift & 7));

						Storage<personality>::sprite_cache_[index][c] =
							(sprite.image[2] & 0xf & sprite_colour_selection_masks[bit]) |
							uint8_t((bit << StatusSpriteCollisionShift) & sprite.collision_bit());
					}
				}
			}

			// Go backwards compositing any sprites that are set as OR masks onto their parents.
			for(int index = buffer.active_sprite_slot - 1; index >= min_sprite + 1; --index) {
				auto &sprite = buffer.active_sprites[index];
				if(sprite.opaque()) {
					continue;
				}

				// Sprite may affect all previous up to and cindlugin the next one that is opaque.
				for(int previous_index = index - 1; previous_index >= min_sprite; --previous_index) {
					// Determine region of overlap (if any).
					auto &previous = buffer.active_sprites[previous_index];
					const int origin = sprite.x - previous.x;
					const int x1 = std::max(0, -origin);
					const int x2 = std::min(pixel_width - origin, pixel_width);

					// Composite sprites.
					for(int x = x1; x < x2; x++) {
						Storage<personality>::sprite_cache_[previous_index][x + origin]
							|= Storage<personality>::sprite_cache_[index][x];
					}

					// If a previous opaque sprite has been found, stop.
					if(previous.opaque()) {
						break;
					}
				}
			}
		}

		// Draw.
		for(int index = buffer.active_sprite_slot - 1; index >= min_sprite; --index) {
			auto &sprite = buffer.active_sprites[index];
			const int x1 = std::max(0, start - sprite.x);
			const int x2 = std::min(end - sprite.x, pixel_width);

			for(int x = x1; x < x2; x++) {
				const uint8_t colour = Storage<personality>::sprite_cache_[index][x];

				// Plot colour, if visible.
				if(colour) {
					pixel_origin_[sprite.x + x] = palette[colour & 0xf];
				}

				// TODO: is collision location recorded in mode 1?

				// Check for a new collision.
				if(!(status_ & StatusSpriteCollision)) {
					sprite_collision |= sprite_buffer[sprite.x + x];
					sprite_buffer[sprite.x + x] |= colour;
					status_ |= sprite_collision & StatusSpriteCollision;

					if(status_ & StatusSpriteCollision) {
						Storage<personality>::collision_location_[0] = uint16_t(x);
						Storage<personality>::collision_location_[1] = uint16_t(y);
					}
				}
			}
		}

		return;
	}

	if constexpr (mode == SpriteMode::Mode1) {
		for(int index = buffer.active_sprite_slot - 1; index >= min_sprite; --index) {
			auto &sprite = buffer.active_sprites[index];
			if(sprite.shift_position >= shifter_target) {
				continue;
			}

			const int pixel_start = std::max(start, sprite.x);
			for(int c = pixel_start; c < end && sprite.shift_position < shifter_target; ++c) {
				const int shift = (sprite.shift_position >> 1) ^ 7;
				int sprite_colour = (sprite.image[shift >> 3] >> (shift & 7)) & 1;

				// A colision is detected regardless of sprite colour ...
				sprite_collision |= sprite_buffer[c] & sprite_colour;
				sprite_buffer[c] |= sprite_colour;

				// ... but a sprite with the transparent colour won't actually be visible.
				sprite_colour &= colour_masks[sprite.image[2] & 0xf];

				pixel_origin_[c] =
					(pixel_origin_[c] & sprite_colour_selection_masks[sprite_colour^1]) |
					(palette[sprite.image[2] & 0xf] & sprite_colour_selection_masks[sprite_colour]);

				sprite.shift_position += shift_advance;
			}
		}

		status_ |= sprite_collision << StatusSpriteCollisionShift;
		return;
	}
}

// Mode 2 logic, as I currently understand it, as a note for my future self:
//
//	If a sprite is marked as 'CC' then it doesn't collide, but its colour value is
//	ORd with those of all lower-numbered sprites down to the next one that is visible on
//	that line and not marked CC.
//
//	If no previous sprite meets that criteria, no pixels are displayed. But if one does
//	then pixels are displayed even where they don't overlap with the earlier sprites.
//
//	... so in terms of my loop above, I guess I need temporary storage to accumulate
//	an OR mask up until I hit a non-CC sprite, at which point I composite everything out?
//	I'm not immediately sure whether I can appropriately reuse sprite_buffer, but possibly?

// MARK: - TMS9918

template <Personality personality>
template <SpriteMode sprite_mode>
void Base<personality>::draw_tms_character(const int start, const int end) {
	auto &line_buffer = *draw_line_buffer_;

	// Paint the background tiles.
	const int pixels_left = end - start;
	if(this->screen_mode_ == ScreenMode::MultiColour) {
		for(int c = start; c < end; ++c) {
			pixel_target_[c] = palette()[
				(line_buffer.tiles.patterns[c >> 3][0] >> (((c & 4)^4))) & 15
			];
		}
	} else {
		const int shift = start & 7;
		int byte_column = start >> 3;

		int length = std::min(pixels_left, 8 - shift);

		int pattern = Numeric::bit_reverse(line_buffer.tiles.patterns[byte_column][0]) >> shift;
		uint8_t colour = line_buffer.tiles.patterns[byte_column][1];
		uint32_t colours[2] = {
			palette()[(colour & 15) ? (colour & 15) : background_colour_],
			palette()[(colour >> 4) ? (colour >> 4) : background_colour_]
		};

		int background_pixels_left = pixels_left;
		while(true) {
			background_pixels_left -= length;
			for(int c = 0; c < length; ++c) {
				pixel_target_[c] = colours[pattern&0x01];
				pattern >>= 1;
			}
			pixel_target_ += length;

			if(!background_pixels_left) break;
			length = std::min(8, background_pixels_left);
			byte_column++;

			pattern = Numeric::bit_reverse(line_buffer.tiles.patterns[byte_column][0]);
			colour = line_buffer.tiles.patterns[byte_column][1];
			colours[0] = palette()[(colour & 15) ? (colour & 15) : background_colour_];
			colours[1] = palette()[(colour >> 4) ? (colour >> 4) : background_colour_];
		}
	}

	draw_sprites<sprite_mode, false>(0, start, end, palette());	// TODO: propagate a real 'y' into here.
}

template <Personality personality>
template <bool apply_blink>
void Base<personality>::draw_tms_text(const int start, const int end) {
	auto &line_buffer = *draw_line_buffer_;
	uint32_t colours[2][2] = {
		{palette()[background_colour_], palette()[text_colour_]},
		{0, 0}
	};
	if constexpr (apply_blink) {
		colours[1][0] = palette()[Storage<personality>::blink_background_colour_];
		colours[1][1] = palette()[Storage<personality>::blink_text_colour_];
	}

	const int shift = start % 6;
	int byte_column = start / 6;
	int pattern = Numeric::bit_reverse(line_buffer.characters.shapes[byte_column]) >> shift;
	int pixels_left = end - start;
	int length = std::min(pixels_left, 6 - shift);
	int flag = 0;
	if constexpr (apply_blink) {
		flag = (line_buffer.characters.flags[byte_column >> 3] >> ((byte_column & 7) ^ 7)) & Storage<personality>::in_blink_;
	}
	while(true) {
		pixels_left -= length;
		for(int c = 0; c < length; ++c) {
			pixel_target_[c] = colours[flag][(pattern&0x01)];
			pattern >>= 1;
		}
		pixel_target_ += length;

		if(!pixels_left) break;
		length = std::min(6, pixels_left);
		byte_column++;
		pattern = Numeric::bit_reverse(line_buffer.characters.shapes[byte_column]);
		if constexpr (apply_blink) {
			flag = (line_buffer.characters.flags[byte_column >> 3] >> ((byte_column & 7) ^ 7)) & Storage<personality>::in_blink_;
		}
	}
}

// MARK: - Master System

template <Personality personality>
void Base<personality>::draw_sms(
	[[maybe_unused]] const int start,
	[[maybe_unused]] const int end,
	[[maybe_unused]] const uint32_t cram_dot
) {
	if constexpr (is_sega_vdp(personality)) {
		int colour_buffer[256];
		auto &line_buffer = *draw_line_buffer_;

		/*
			Add extra border for any pixels that fall before the fine scroll.
		*/
		int tile_start = start, tile_end = end;
		int tile_offset = start;
		if(output_pointer_.row >= 16 || !Storage<personality>::horizontal_scroll_lock_) {
			for(int c = start; c < (line_buffer.latched_horizontal_scroll & 7); ++c) {
				colour_buffer[c] = 16 + background_colour_;
				++tile_offset;
			}

			// Remove the border area from that to which tiles will be drawn.
			tile_start = std::max(start - (line_buffer.latched_horizontal_scroll & 7), 0);
			tile_end = std::max(end - (line_buffer.latched_horizontal_scroll & 7), 0);
		}


		uint32_t pattern;
		uint8_t *const pattern_index = reinterpret_cast<uint8_t *>(&pattern);

		/*
			Add background tiles; these will fill the colour_buffer with values in which
			the low five bits are a palette index, and bit six is set if this tile has
			priority over sprites.
		*/
		if(tile_start < end) {
			const int shift = tile_start & 7;
			int byte_column = tile_start >> 3;
			int pixels_left = tile_end - tile_start;
			int length = std::min(pixels_left, 8 - shift);

			pattern = *reinterpret_cast<const uint32_t *>(line_buffer.tiles.patterns[byte_column]);
			if(line_buffer.tiles.flags[byte_column]&2)
				pattern >>= shift;
			else
				pattern <<= shift;

			while(true) {
				const int palette_offset = (line_buffer.tiles.flags[byte_column]&0x18) << 1;
				if(line_buffer.tiles.flags[byte_column]&2) {
					for(int c = 0; c < length; ++c) {
						colour_buffer[tile_offset] =
							((pattern_index[3] & 0x01) << 3) |
							((pattern_index[2] & 0x01) << 2) |
							((pattern_index[1] & 0x01) << 1) |
							((pattern_index[0] & 0x01) << 0) |
							palette_offset;
						++tile_offset;
						pattern >>= 1;
					}
				} else {
					for(int c = 0; c < length; ++c) {
						colour_buffer[tile_offset] =
							((pattern_index[3] & 0x80) >> 4) |
							((pattern_index[2] & 0x80) >> 5) |
							((pattern_index[1] & 0x80) >> 6) |
							((pattern_index[0] & 0x80) >> 7) |
							palette_offset;
						++tile_offset;
						pattern <<= 1;
					}
				}

				pixels_left -= length;
				if(!pixels_left) break;

				length = std::min(8, pixels_left);
				byte_column++;
				pattern = *reinterpret_cast<const uint32_t *>(line_buffer.tiles.patterns[byte_column]);
			}
		}

		/*
			Apply sprites (if any).
		*/
		draw_sprites<SpriteMode::MasterSystem, false>(0, start, end, palette(), colour_buffer);	// TODO provide good y, as per elsewhere.

		// Map from the 32-colour buffer to real output pixels, applying the specific CRAM dot if any.
		pixel_target_[start] = Storage<personality>::colour_ram_[colour_buffer[start] & 0x1f] | cram_dot;
		for(int c = start+1; c < end; ++c) {
			pixel_target_[c] = Storage<personality>::colour_ram_[colour_buffer[c] & 0x1f];
		}

		// If the VDP is set to hide the left column and this is the final call that'll come
		// this line, hide it.
		if(end == 256) {
			if(Storage<personality>::hide_left_column_) {
				pixel_origin_[0] = pixel_origin_[1] = pixel_origin_[2] = pixel_origin_[3] =
				pixel_origin_[4] = pixel_origin_[5] = pixel_origin_[6] = pixel_origin_[7] =
					Storage<personality>::colour_ram_[16 + background_colour_];
			}
		}
	}
}

// MARK: - Yamaha

template <Personality personality>
template <ScreenMode mode>
void Base<personality>::draw_yamaha(const uint8_t y, int start, int end) {
	[[maybe_unused]] const auto active_palette = palette();
	const int sprite_start = start >> 2;
	const int sprite_end = end >> 2;
	auto &line_buffer = *draw_line_buffer_;

	// Observation justifying Duff's device below: it's acceptable to paint too many pixels — to paint
	// beyond `end` — provided that the overpainting is within normal bitmap bounds, because any
	// mispainted pixels will be replaced before becoming visible to the user.

	if constexpr (mode == ScreenMode::YamahaGraphics4 || mode == ScreenMode::YamahaGraphics6) {
		start >>= (mode == ScreenMode::YamahaGraphics4) ? 2 : 1;
		end >>= (mode == ScreenMode::YamahaGraphics4) ? 2 : 1;

		int column = start & ~1;
		const int offset = start & 1;
		start >>= 1;
		end = (end + 1) >> 1;

		switch(offset) {
			case 0:
				do {
						pixel_target_[column+0] = active_palette[line_buffer.bitmap[start] >> 4];	[[fallthrough]];
			case 1:		pixel_target_[column+1] = active_palette[line_buffer.bitmap[start] & 0xf];
						++start;
						column += 2;
				} while(start < end);
		}
	}

	if constexpr (mode == ScreenMode::YamahaGraphics5) {
		start >>= 1;
		end >>= 1;

		int column = start & ~3;
		const int offset = start & 3;
		start >>= 2;
		end = (end + 3) >> 2;

		switch(offset) {
			case 0:
				do {
					pixel_target_[column+0] = active_palette[line_buffer.bitmap[start] >> 6];			[[fallthrough]];
			case 1:	pixel_target_[column+1] = active_palette[(line_buffer.bitmap[start] >> 4) & 3];		[[fallthrough]];
			case 2:	pixel_target_[column+2] = active_palette[(line_buffer.bitmap[start] >> 2) & 3];		[[fallthrough]];
			case 3:	pixel_target_[column+3] = active_palette[line_buffer.bitmap[start] & 3];
					++start;
					column += 4;
				} while(start < end);
		}
	}

	if constexpr (mode == ScreenMode::YamahaGraphics7) {
		start >>= 2;
		end >>= 2;

		while(start < end) {
			pixel_target_[start] =
				palette_pack(
					uint8_t((line_buffer.bitmap[start] & 0x1c) + ((line_buffer.bitmap[start] & 0x1c) << 3) + ((line_buffer.bitmap[start] & 0x1c) >> 3)),
					uint8_t((line_buffer.bitmap[start] & 0xe0) + ((line_buffer.bitmap[start] & 0xe0) >> 3) + ((line_buffer.bitmap[start] & 0xe0) >> 6)),
					uint8_t((line_buffer.bitmap[start] & 0x03) + ((line_buffer.bitmap[start] & 0x03) << 2) + ((line_buffer.bitmap[start] & 0x03) << 4) + ((line_buffer.bitmap[start] & 0x03) << 6))
				);
			++start;
		}
	}

	constexpr std::array<uint32_t, 16> graphics7_sprite_palette = {
		palette_pack(0b00000000, 0b00000000, 0b00000000),	palette_pack(0b00000000, 0b00000000, 0b01001001),
		palette_pack(0b00000000, 0b01101101, 0b00000000),	palette_pack(0b00000000, 0b01101101, 0b01001001),
		palette_pack(0b01101101, 0b00000000, 0b00000000),	palette_pack(0b01101101, 0b00000000, 0b01001001),
		palette_pack(0b01101101, 0b01101101, 0b00000000),	palette_pack(0b01101101, 0b01101101, 0b01001001),

		palette_pack(0b10010010, 0b11111111, 0b01001001),	palette_pack(0b00000000, 0b00000000, 0b11111111),
		palette_pack(0b00000000, 0b11111111, 0b00000000),	palette_pack(0b00000000, 0b11111111, 0b11111111),
		palette_pack(0b11111111, 0b00000000, 0b00000000),	palette_pack(0b11111111, 0b00000000, 0b11111111),
		palette_pack(0b11111111, 0b11111111, 0b00000000),	palette_pack(0b11111111, 0b11111111, 0b11111111),
	};

	// Possibly TODO: is the data-sheet trying to allege some sort of colour mixing for sprites in Mode 6?
	draw_sprites<
		SpriteMode::Mode2,
		mode == ScreenMode::YamahaGraphics5 || mode == ScreenMode::YamahaGraphics6
	>(y, sprite_start, sprite_end, mode == ScreenMode::YamahaGraphics7 ? graphics7_sprite_palette : palette());
}

template <Personality personality>
void Base<personality>::draw_yamaha(const uint8_t y, const int start, const int end) {
	if constexpr (is_yamaha_vdp(personality)) {
		switch(draw_line_buffer_->screen_mode) {
			// Modes that are the same (or close enough) to those on the TMS.
			case ScreenMode::Text:			draw_tms_text<false>(start >> 2, end >> 2);	break;
			case ScreenMode::YamahaText80:	draw_tms_text<true>(start >> 1, end >> 1);	break;
			case ScreenMode::MultiColour:
			case ScreenMode::ColouredText:
			case ScreenMode::Graphics:		draw_tms_character(start >> 2, end >> 2);	break;

			case ScreenMode::YamahaGraphics3:
				draw_tms_character<SpriteMode::Mode2>(start >> 2, end >> 2);
			break;

#define Dispatch(x)	case ScreenMode::x:	draw_yamaha<ScreenMode::x>(y, start, end);	break;
			Dispatch(YamahaGraphics4);
			Dispatch(YamahaGraphics5);
			Dispatch(YamahaGraphics6);
			Dispatch(YamahaGraphics7);
#undef Dispatch

			default: break;
		}
	}
}

// MARK: - Mega Drive

// TODO.

}
