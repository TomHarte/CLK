//
//  9918.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 25/11/2017.
//  Copyright © 2017 Thomas Harte. All rights reserved.
//

#include "9918.hpp"

#include <cstring>

using namespace TI;

namespace {

const uint32_t palette_pack(uint8_t r, uint8_t g, uint8_t b) {
	uint32_t result = 0;
	uint8_t *result_ptr = reinterpret_cast<uint8_t *>(&result);
	result_ptr[0] = r;
	result_ptr[1] = g;
	result_ptr[2] = b;
	result_ptr[3] = 0;
	return result;
}

const uint32_t palette[16] = {
	palette_pack(0, 0, 0),
	palette_pack(0, 0, 0),
	palette_pack(90, 201, 81),
	palette_pack(149, 231, 133),

	palette_pack(113, 104, 183),
	palette_pack(146, 132, 255),
	palette_pack(200, 114, 89),
	palette_pack(115, 222, 255),

	palette_pack(238, 124, 90),
	palette_pack(255, 166, 132),
	palette_pack(219, 232, 92),
	palette_pack(240, 247, 143),

	palette_pack(78, 176, 63),
	palette_pack(202, 118, 216),
	palette_pack(233, 233, 233),
	palette_pack(255, 255, 255)
};

}

TMS9918::TMS9918(Personality p) :
	crt_(new Outputs::CRT::CRT(342, 1, Outputs::CRT::DisplayType::NTSC60, 4)) {
	crt_->set_rgb_sampling_function(
		"vec3 rgb_sample(usampler2D sampler, vec2 coordinate, vec2 icoordinate)"
		"{"
			"return texture(sampler, coordinate).rgb / vec3(255.0);"
		"}");
	crt_->set_output_device(Outputs::CRT::OutputDevice::Monitor);
	crt_->set_visible_area(Outputs::CRT::Rect(0.055f, 0.025f, 0.9f, 0.9f));
}

std::shared_ptr<Outputs::CRT::CRT> TMS9918::get_crt() {
	return crt_;
}

void TMS9918::test_sprite(int sprite_number) {
	if(sprites_stopped_) return;
	if(!(status_ & 0x40)) {
		status_ = static_cast<uint8_t>((status_ & ~31) | sprite_number);
	}

	const int sprite_position = ram_[sprite_attribute_table_address_ + (sprite_number << 2)];
	// A sprite Y of 208 means "don't scan the list any further".
	if(sprite_position == 208) {
		sprites_stopped_ = true;
		return;
	}

	const int sprite_row = (row_ - sprite_position)&255;
	int sprite_height = 8;
	if(sprites_16x16_) sprite_height <<= 1;
	if(sprites_magnified_) sprite_height <<= 1;
	if(sprite_row < 0 || sprite_row >= sprite_height) return;

	if(active_sprite_slot_ == 4) {
		status_ |= 0x40;
		return;
	}

	active_sprites_[active_sprite_slot_].index = sprite_number;
	active_sprites_[active_sprite_slot_].row = sprite_row;
	active_sprite_slot_++;
}

void TMS9918::get_sprite_contents(int field, int cycles_left) {
	int sprite = 2 + (field / 6);
	field %= 6;

	while(true) {
		const int cycles_in_sprite = std::min(cycles_left, 6 - field);
		cycles_left -= cycles_in_sprite;
		const int final_field = cycles_in_sprite + field;

		if(field < 4) {
			std::memcpy(
				&active_sprites_[sprite].info[field],
				&ram_[sprite_attribute_table_address_ + (active_sprites_[sprite].index << 2) + field],
				static_cast<size_t>(std::min(4, final_field) - field));
		}

		field = std::min(4, final_field);
		const int sprite_offset = active_sprites_[sprite].info[2] & ~(sprites_16x16_ ? 3 : 0);
		const int sprite_address =
			sprite_generator_table_address_ + (sprite_offset << 3) + active_sprites_[sprite].row;
		while(field < final_field) {
			active_sprites_[sprite].image[field - 4] = ram_[sprite_address + ((field - 4) << 4)];
			field++;
		}

		if(!cycles_left) return;
		field = 0;
		sprite++;
	}
}

void TMS9918::run_for(const HalfCycles cycles) {
	// As specific as I've been able to get:
	// Scanline time is always 227.75 cycles.
	// PAL output is 313 lines total. NTSC output is 262 lines total.
	// Interrupt is signalled upon entering the lower border.

	// Keep a count of cycles separate from internal counts to avoid
	// potential errors mapping back and forth.
	half_cycles_into_frame_ = (half_cycles_into_frame_ + cycles) % HalfCycles(frame_lines_ * 228 * 2);

	// Convert to 342 cycles per line; the internal clock is 1.5 times the
	// nominal 3.579545 Mhz that I've advertised for this part.
	int int_cycles = (cycles.as_int() * 3) + cycles_error_;
	cycles_error_ = int_cycles & 7;
	int_cycles >>= 3;
	if(!int_cycles) return;

	//
	// Break that down as:
	// 26 cycles sync;

	while(int_cycles) {
		// Determine how much time has passed in the remainder of this line, and proceed.
		int cycles_left = std::min(342 - column_, int_cycles);
		column_ += cycles_left;		// column_ is now the column that has been reached in this line.
		int_cycles -= cycles_left;	// Count down duration to run for.



		// ------------------------------
		// TODO: memory access slot here.
		// ------------------------------



		// ------------------------------
		// Perform video memory accesses.
		// ------------------------------
		if(row_	< 192 && !blank_screen_) {
			const int access_slot = column_ >> 1;	// There are only 171 available memory accesses per line.
			switch(line_mode_) {
				case LineMode::Text:
					access_pointer_ = std::min(30, access_slot);
					if(access_pointer_ >= 30 && access_pointer_ < 150) {
						const int row_base = pattern_name_address_ + (row_ >> 3) * 40;
						const int end = std::min(150, access_slot);

						// Pattern names are collected every third window starting from window 30.
						const int pattern_names_start = (access_pointer_ - 30 + 2) / 3;
						const int pattern_names_end = (end - 30 + 2) / 3;
						std::memcpy(&pattern_names_[pattern_names_start], &ram_[row_base + pattern_names_start], static_cast<size_t>(pattern_names_end - pattern_names_start));

						// Patterns are collected every third window starting from window 32.
						const int pattern_buffer_start = (access_pointer_ - 32 + 2) / 3;
						const int pattern_buffer_end = (end - 32 + 2) / 3;
						for(int column = pattern_buffer_start; column < pattern_buffer_end; ++column) {
							pattern_buffer_[column] = ram_[pattern_generator_table_address_ + (pattern_names_[column] << 3) + (row_ & 7)];
						}
					}
				break;

				case LineMode::Character:
					// Four access windows: no collection.
					access_pointer_ = std::min(5, access_slot);

					// Then ten access windows are filled with collection of sprite 3 and 4 details.
					if(access_pointer_ >= 5 && access_pointer_ < 15) {
						get_sprite_contents(access_pointer_ - 5 + 14, 15 - access_pointer_);
						access_pointer_ = std::min(15, access_slot);
					}

					// Four more access windows: no collection.
					access_pointer_ = std::min(19, access_slot);

					// Then eight access windows fetch the y position for the first eight sprites.
					while(access_pointer_ < 27 && access_pointer_ < access_slot) {
						test_sprite(access_pointer_ - 19);
						access_pointer_++;
					}

					// The next 128 access slots are video and sprite collection interleaved.
					if(access_pointer_ >= 27 && access_pointer_ < 155) {
						int end = std::min(155, access_slot);

						int row_base = pattern_name_address_;
						int pattern_base = pattern_generator_table_address_;
						int colour_base = colour_table_address_;
						if(screen_mode_ == 1) {
							pattern_base &= 0x2000 | ((row_ & 0xc0) << 5);
							colour_base &= 0x2000 | ((row_ & 0xc0) << 5);
						}
						row_base += (row_ << 2)&~31;

						// Pattern names are collected every fourth window starting from window 27.
						const int pattern_names_start = (access_pointer_ - 27 + 3) >> 2;
						const int pattern_names_end = (end - 27 + 3) >> 2;
						std::memcpy(&pattern_names_[pattern_names_start], &ram_[row_base + pattern_names_start], static_cast<size_t>(pattern_names_end - pattern_names_start));

						// Colours are collected ever fourth window starting from window 29.
						const int colours_start = (access_pointer_ - 29 + 3) >> 2;
						const int colours_end = (end - 29 + 3) >> 2;
						if(screen_mode_ != 1) {
							for(int column = colours_start; column < colours_end; ++column) {
								colour_buffer_[column] = ram_[colour_base + (pattern_names_[column] >> 3)];
							}
						} else {
							for(int column = colours_start; column < colours_end; ++column) {
								colour_buffer_[column] = ram_[colour_base + (pattern_names_[column] << 3) + (row_ & 7)];
							}
						}

						// Patterns are collected ever fourth window starting from window 30.
						const int pattern_buffer_start = (access_pointer_ - 30 + 3) >> 2;
						const int pattern_buffer_end = (end - 30 + 3) >> 2;
						for(int column = pattern_buffer_start; column < pattern_buffer_end; ++column) {
							pattern_buffer_[column] = ram_[pattern_base + (pattern_names_[column] << 3) + (row_ & 7)];
						}

						// Sprite slots occur in three quarters of ever fourth window starting from window 32.
						const int sprite_start = (access_pointer_ - 32 + 3) >> 2;
						const int sprite_end = (end - 32 + 3) >> 2;
						for(int column = sprite_start; column < sprite_end; ++column) {
							if((column&3) != 3) {
								test_sprite(8 + column - (column >> 2));
							}
						}

						access_pointer_ = std::min(154, access_pointer_);
					}

					// Two access windows: no collection.
					access_pointer_ = std::min(157, access_slot);

					// Fourteen access windows: collect initial sprite information.
					if(access_pointer_ >= 157 && access_pointer_ < 170) {
						get_sprite_contents(access_pointer_ - 157, 170 - access_pointer_);
						access_pointer_ = std::min(170, access_slot);
					}
				break;
			}
		}
		// --------------------------
		// End video memory accesses.
		// --------------------------



		// --------------------
		// Output video stream.
		// --------------------
		if(row_	< 192 && !blank_screen_) {
			// ----------------------
			// Output horizontal sync
			// ----------------------
			if(!output_column_ && column_ >= 26) {
				crt_->output_sync(static_cast<unsigned int>(26));
				output_column_ = 26;
			}

			// --------------------------
			// TODO: output colour burst.
			// --------------------------


			// -------------------
			// Output left border.
			// -------------------
			if(output_column_ >= 26) {
				int pixels_end = std::min(first_pixel_column_, column_);
				if(output_column_ < pixels_end) {
					output_border(pixels_end - output_column_);
					output_column_ = pixels_end;

					// Grab a pointer for drawing pixels to, if the moment has arrived.
					if(pixels_end == first_pixel_column_) {
						pixel_base_ = pixel_target_ = reinterpret_cast<uint32_t *>(crt_->allocate_write_area(static_cast<unsigned int>(first_right_border_column_ - first_pixel_column_)));
					}
				}
			}

			// --------------
			// Output pixels.
			// --------------
			if(output_column_ >= first_pixel_column_) {
				int pixels_end = std::min(first_right_border_column_, column_);

				if(output_column_ < pixels_end) {
					switch(line_mode_) {
						case LineMode::Text: {
							const uint32_t colours[2] = { palette[background_colour_], palette[text_colour_] };

							const int shift = (output_column_ - first_pixel_column_) % 6;
							int byte_column = (output_column_ - first_pixel_column_) / 6;
							int pattern = pattern_buffer_[byte_column] << shift;
							int pixels_left = pixels_end - output_column_;
							int length = std::min(pixels_left, 6 - shift);
							while(true) {
								pixels_left -= length;
								while(length--) {
									*pixel_target_ = colours[(pattern >> 7)&0x01];
									pixel_target_++;
									pattern <<= 1;
								}

								if(!pixels_left) break;
								length = std::min(6, pixels_left);
								byte_column++;
								pattern = pattern_buffer_[byte_column];
							}
							output_column_ = pixels_end;
						} break;

						case LineMode::Character:
							while(output_column_ < pixels_end) {
								int base = (output_column_ - first_pixel_column_);
								int address = base >> 3;
								uint8_t colour = colour_buffer_[address];
								uint8_t pattern = pattern_buffer_[address];
								pattern >>= ((base&7)^7);

								*pixel_target_ = (pattern&1) ? palette[colour >> 4] : palette[colour & 15];
								pixel_target_ ++;
								output_column_ ++;
							}
						break;
					}

					if(output_column_ == first_right_border_column_) {
						// Just chuck the sprites on. Quick hack!
						for(size_t c = 0; c < 4; ++c) {
							const size_t sprite_index = c^3;
							if(static_cast<int>(sprite_index) >= active_sprite_slot_) continue;
							if(!(active_sprites_[sprite_index].info[3]&15)) continue;

							for(int p = 0; p < (sprites_16x16_ ? 16 : 8); ++p) {
								int x = active_sprites_[sprite_index].info[1] + p;
								if(active_sprites_[sprite_index].info[3] & 0x80) x -= 32;
								if(x >= 0 && x < 256) {
									if(((active_sprites_[sprite_index].image[p >> 3] << (p&7)) & 0x80)) pixel_base_[x] = palette[active_sprites_[sprite_index].info[3]&15];
								}
							}
						}

						crt_->output_data(static_cast<unsigned int>(first_right_border_column_ - first_pixel_column_), 1);
						pixel_target_ = nullptr;
					}
				}
			}

			// --------------------
			// Output right border.
			// --------------------
			if(output_column_ >= first_right_border_column_) {
				output_border(column_ - output_column_);
				output_column_ = column_;
			}
		} else if(row_ >= first_vsync_line_ && row_ < first_vsync_line_+3) {
			// Vertical sync.
			if(column_ == 342) {
				crt_->output_sync(static_cast<unsigned int>(342));
			}
		} else {
			// Blank.
			if(!output_column_ && column_ >= 26) {
				crt_->output_sync(static_cast<unsigned int>(26));
				output_column_ = 26;
			}
			if(output_column_ >= 26) {
				output_border(column_ - output_column_);
				output_column_ = column_;
			}
		}
		// -----------------
		// End video stream.
		// -----------------



		// -----------------------------------
		// Prepare for next line, potentially.
		// -----------------------------------
		if(column_ == 342) {
			access_pointer_ = column_ = output_column_ = 0;
			row_ = (row_ + 1) % frame_lines_;
			if(row_ == 192) status_ |= 0x80;
			active_sprite_slot_ = 0;
			sprites_stopped_ = false;

			screen_mode_ = next_screen_mode_;
			blank_screen_ = next_blank_screen_;
			switch(screen_mode_) {
				case 2:
					line_mode_ = LineMode::Text;
					first_pixel_column_ = 69;
					first_right_border_column_ = 309;
				break;
				default:
					line_mode_ = LineMode::Character;
					first_pixel_column_ = 63;
					first_right_border_column_ = 319;
				break;
			}
		}
	}
}

void TMS9918::output_border(int cycles) {
	pixel_target_ = reinterpret_cast<uint32_t *>(crt_->allocate_write_area(1));
	if(pixel_target_) *pixel_target_ = palette[background_colour_];
	crt_->output_level(static_cast<unsigned int>(cycles));
}

// TODO: as a temporary development measure, memory access below is magically instantaneous. Correct that.

void TMS9918::set_register(int address, uint8_t value) {
	// Writes to address 0 are writes to the video RAM. Store
	// the value and return.
	if(!(address & 1)) {
		write_phase_ = false;
		read_ahead_buffer_ = value;
		ram_[ram_pointer_ & 16383] = value;
		ram_pointer_++;
		return;
	}

	// Writes to address 1 are performed in pairs; if this is the
	// low byte of a value, store it and wait for the high byte.
	if(!write_phase_) {
		low_write_ = value;
		write_phase_ = true;
		return;
	}

	write_phase_ = false;
	if(value & 0x80) {
		// This is a write to a register.
		switch(value & 7) {
			case 0:
				next_screen_mode_ = (next_screen_mode_ & 6) | ((low_write_ & 2) >> 1);
//				printf("NSM: %02x\n", next_screen_mode_);
			break;

			case 1:
				next_blank_screen_ = !(low_write_ & 0x40);
				generate_interrupts_ = !!(low_write_ & 0x20);
				next_screen_mode_ = (next_screen_mode_ & 1) | ((low_write_ & 0x18) >> 3);
				sprites_16x16_ = !!(low_write_ & 0x02);
				sprites_magnified_ = !!(low_write_ & 0x01);
//				printf("NSM: %02x\n", next_screen_mode_);
			break;

			case 2:
				pattern_name_address_ = static_cast<uint16_t>((low_write_ & 0xf) << 10);
			break;

			case 3:
				colour_table_address_ = static_cast<uint16_t>(low_write_ << 6);
			break;

			case 4:
				pattern_generator_table_address_ = static_cast<uint16_t>((low_write_ & 0x07) << 11);
			break;

			case 5:
				sprite_attribute_table_address_ = static_cast<uint16_t>((low_write_ & 0x7f) << 7);
			break;

			case 6:
				sprite_generator_table_address_ = static_cast<uint16_t>((low_write_ & 0x07) << 11);
			break;

			case 7:
				text_colour_ = low_write_ >> 4;
				background_colour_ = low_write_ & 0xf;
			break;
		}
	} else {
		// This is a write to the RAM pointer.
		ram_pointer_ = static_cast<uint16_t>(low_write_ | (value << 8));
		if(!(value & 0x40)) {
			// Officially a 'read' set, so perform lookahead.
			read_ahead_buffer_ = ram_[ram_pointer_ & 16383];
			ram_pointer_++;
		}
	}
}

uint8_t TMS9918::get_register(int address) {
	write_phase_ = false;

	// Reads from address 0 read video RAM, via the read-ahead buffer.
	if(!(address & 1)) {
		uint8_t result = read_ahead_buffer_;
		read_ahead_buffer_ = ram_[ram_pointer_ & 16383];
		ram_pointer_++;
		return result;
	}

	// Reads from address 1 get the status register.
	uint8_t result = status_;
	status_ &= ~(0x80 | 0x40 | 0x20);
	return result;
}

 HalfCycles TMS9918::get_time_until_interrupt() {
	if(!generate_interrupts_) return HalfCycles(-1);
	if(get_interrupt_line()) return HalfCycles(0);

	const int half_cycles_per_frame = frame_lines_ * 228 * 2;
	int half_cycles_remaining = (192 * 228 * 2 + half_cycles_per_frame - half_cycles_into_frame_.as_int()) % half_cycles_per_frame;
	return HalfCycles(half_cycles_remaining);
}

bool TMS9918::get_interrupt_line() {
	return (status_ & 0x80) && generate_interrupts_;
}
