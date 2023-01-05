//
//  Draw.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 05/01/2023.
//  Copyright © 2023 Thomas Harte. All rights reserved.
//

#ifndef Draw_hpp
#define Draw_hpp

#include "../../../Numeric/BitReverse.hpp"

// MARK: - TMS9918

template <Personality personality>
void Base<personality>::draw_tms_character(int start, int end) {
	LineBuffer &line_buffer = line_buffers_[read_pointer_.row];

	// Paint the background tiles.
	const int pixels_left = end - start;
	if(this->screen_mode_ == ScreenMode::MultiColour) {
		for(int c = start; c < end; ++c) {
			pixel_target_[c] = palette[
				(line_buffer.patterns[c >> 3][0] >> (((c & 4)^4))) & 15
			];
		}
	} else {
		const int shift = start & 7;
		int byte_column = start >> 3;

		int length = std::min(pixels_left, 8 - shift);

		int pattern = Numeric::bit_reverse(line_buffer.patterns[byte_column][0]) >> shift;
		uint8_t colour = line_buffer.patterns[byte_column][1];
		uint32_t colours[2] = {
			palette[(colour & 15) ? (colour & 15) : background_colour_],
			palette[(colour >> 4) ? (colour >> 4) : background_colour_]
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

			pattern = Numeric::bit_reverse(line_buffer.patterns[byte_column][0]);
			colour = line_buffer.patterns[byte_column][1];
			colours[0] = palette[(colour & 15) ? (colour & 15) : background_colour_];
			colours[1] = palette[(colour >> 4) ? (colour >> 4) : background_colour_];
		}
	}

	// Paint sprites and check for collisions, but only if at least one sprite is active
	// on this line.
	if(line_buffer.active_sprite_slot) {
		const int shift_advance = sprites_magnified_ ? 1 : 2;
		// If this is the start of the line clip any part of any sprites that is off to the left.
		if(!start) {
			for(int index = 0; index < line_buffer.active_sprite_slot; ++index) {
				LineBuffer::ActiveSprite &sprite = line_buffer.active_sprites[index];
				if(sprite.x < 0) sprite.shift_position -= shift_advance * sprite.x;
			}
		}

		int sprite_buffer[256];
		int sprite_collision = 0;
		memset(&sprite_buffer[start], 0, size_t(end - start)*sizeof(sprite_buffer[0]));

		constexpr uint32_t sprite_colour_selection_masks[2] = {0x00000000, 0xffffffff};
		constexpr int colour_masks[16] = {0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1};

		// Draw all sprites into the sprite buffer.
		const int shifter_target = sprites_16x16_ ? 32 : 16;
		for(int index = line_buffer.active_sprite_slot - 1; index >= 0; --index) {
			LineBuffer::ActiveSprite &sprite = line_buffer.active_sprites[index];
			if(sprite.shift_position < shifter_target) {
				const int pixel_start = std::max(start, sprite.x);
				for(int c = pixel_start; c < end && sprite.shift_position < shifter_target; ++c) {
					const int shift = (sprite.shift_position >> 1) ^ 7;
					int sprite_colour = (sprite.image[shift >> 3] >> (shift & 7)) & 1;

					// A colision is detected regardless of sprite colour ...
					sprite_collision |= sprite_buffer[c] & sprite_colour;
					sprite_buffer[c] |= sprite_colour;

					// ... but a sprite with the transparent colour won't actually be visible.
					sprite_colour &= colour_masks[sprite.image[2]&15];
					pixel_origin_[c] =
						(pixel_origin_[c] & sprite_colour_selection_masks[sprite_colour^1]) |
						(palette[sprite.image[2]&15] & sprite_colour_selection_masks[sprite_colour]);

					sprite.shift_position += shift_advance;
				}
			}
		}

		status_ |= sprite_collision << StatusSpriteCollisionShift;
	}
}

template <Personality personality>
void Base<personality>::draw_tms_text(int start, int end) {
	LineBuffer &line_buffer = line_buffers_[read_pointer_.row];
	const uint32_t colours[2] = { palette[background_colour_], palette[text_colour_] };

	const int shift = start % 6;
	int byte_column = start / 6;
	int pattern = Numeric::bit_reverse(line_buffer.patterns[byte_column][0]) >> shift;
	int pixels_left = end - start;
	int length = std::min(pixels_left, 6 - shift);
	while(true) {
		pixels_left -= length;
		for(int c = 0; c < length; ++c) {
			pixel_target_[c] = colours[pattern&0x01];
			pattern >>= 1;
		}
		pixel_target_ += length;

		if(!pixels_left) break;
		length = std::min(6, pixels_left);
		byte_column++;
		pattern = Numeric::bit_reverse(line_buffer.patterns[byte_column][0]);
	}
}

// MARK: - Master System

template <Personality personality>
void Base<personality>::draw_sms(int start, int end, uint32_t cram_dot) {
	LineBuffer &line_buffer = line_buffers_[read_pointer_.row];
	int colour_buffer[256];

	/*
		Add extra border for any pixels that fall before the fine scroll.
	*/
	int tile_start = start, tile_end = end;
	int tile_offset = start;
	if(read_pointer_.row >= 16 || !master_system_.horizontal_scroll_lock) {
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

		pattern = *reinterpret_cast<const uint32_t *>(line_buffer.patterns[byte_column]);
		if(line_buffer.names[byte_column].flags&2)
			pattern >>= shift;
		else
			pattern <<= shift;

		while(true) {
			const int palette_offset = (line_buffer.names[byte_column].flags&0x18) << 1;
			if(line_buffer.names[byte_column].flags&2) {
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
			pattern = *reinterpret_cast<const uint32_t *>(line_buffer.patterns[byte_column]);
		}
	}

	/*
		Apply sprites (if any).
	*/
	if(line_buffer.active_sprite_slot) {
		const int shift_advance = sprites_magnified_ ? 1 : 2;

		// If this is the start of the line clip any part of any sprites that is off to the left.
		if(!start) {
			for(int index = 0; index < line_buffer.active_sprite_slot; ++index) {
				LineBuffer::ActiveSprite &sprite = line_buffer.active_sprites[index];
				if(sprite.x < 0) sprite.shift_position -= shift_advance * sprite.x;
			}
		}

		int sprite_buffer[256];
		int sprite_collision = 0;
		memset(&sprite_buffer[start], 0, size_t(end - start)*sizeof(sprite_buffer[0]));

		// Draw all sprites into the sprite buffer.
		for(int index = line_buffer.active_sprite_slot - 1; index >= 0; --index) {
			LineBuffer::ActiveSprite &sprite = line_buffer.active_sprites[index];
			if(sprite.shift_position < 16) {
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
		}

		// Draw the sprite buffer onto the colour buffer, wherever the tile map doesn't have
		// priority (or is transparent).
		for(int c = start; c < end; ++c) {
			if(
				sprite_buffer[c] &&
				(!(colour_buffer[c]&0x20) || !(colour_buffer[c]&0xf))
			) colour_buffer[c] = sprite_buffer[c];
		}

		if(sprite_collision)
			status_ |= StatusSpriteCollision;
	}

	// Map from the 32-colour buffer to real output pixels, applying the specific CRAM dot if any.
	pixel_target_[start] = master_system_.colour_ram[colour_buffer[start] & 0x1f] | cram_dot;
	for(int c = start+1; c < end; ++c) {
		pixel_target_[c] = master_system_.colour_ram[colour_buffer[c] & 0x1f];
	}

	// If the VDP is set to hide the left column and this is the final call that'll come
	// this line, hide it.
	if(end == 256) {
		if(master_system_.hide_left_column) {
			pixel_origin_[0] = pixel_origin_[1] = pixel_origin_[2] = pixel_origin_[3] =
			pixel_origin_[4] = pixel_origin_[5] = pixel_origin_[6] = pixel_origin_[7] =
				master_system_.colour_ram[16 + background_colour_];
		}
	}
}

// MARK: - Yamaha

// TODO.

// MARK: - Mega Drive

// TODO.

#endif /* Draw_hpp */
