//
//  Nick.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 14/06/2021.
//  Copyright © 2021 Thomas Harte. All rights reserved.
//

#include "Nick.hpp"

#include <cstdio>

namespace  {

uint16_t mapped_colour(uint8_t source) {
	// On the Enterprise, red and green are 3-bit quantities; blue is a 2-bit quantity.
	int red		= ((source&0x01) << 2) | ((source&0x08) >> 2) | ((source&0x40) >> 6);
	int green	= ((source&0x02) << 1) | ((source&0x10) >> 3) | ((source&0x80) >> 7);
	int blue	= ((source&0x04) >> 1) | ((source&0x20) >> 5);

	assert(red <= 7);
	assert(green <= 7);
	assert(blue <= 3);

	red = (red << 1) + (red >> 3);
	green = (green << 1) + (green >> 3);
	blue = (blue << 2) + blue;

	assert(red <= 15);
	assert(green <= 15);
	assert(blue <= 15);

	// Duplicate bits where necessary to map to a full 4-bit range per channel.
	const uint8_t parts[2] = {
		uint8_t(
			red
		),
		uint8_t(
			(green << 4) + blue
		)
	};
	return *reinterpret_cast<const uint16_t *>(parts);
}

}

using namespace Enterprise;

Nick::Nick(const uint8_t *ram) :
	crt_(57*16, 16, Outputs::Display::Type::PAL50, Outputs::Display::InputDataType::Red4Green4Blue4),
	ram_(ram) {

	// Just use RGB for now.
	crt_.set_display_type(Outputs::Display::DisplayType::RGB);

	// Crop to the centre 90% of the display.
	crt_.set_visible_area(Outputs::Display::Rect(0.05f, 0.05f, 0.9f, 0.9f));
}

void Nick::write(uint16_t address, uint8_t value) {
	switch(address & 3) {
		case 0:
			// Ignored: everything to do with external colour.
			for(int c = 0; c < 8; c++) {
				palette_[c + 8] = mapped_colour(uint8_t(((value & 0x1f) << 3) + c));
			}
		break;
		case 1:
			if(output_type_ == OutputType::Border) {
				set_output_type(OutputType::Border, true);
			}
			border_colour_ = mapped_colour(value);
		break;
		case 2:
			line_parameter_base_ = uint16_t((line_parameter_base_ & 0xf000) | (value << 4));
		break;
		case 3:
			line_parameter_base_ = uint16_t((line_parameter_base_ & 0x0ff0) | (value << 12));

			// Still a mystery to me: the exact meaning of the top two bits here. For now
			// just treat a 0 -> 1 transition of the MSB as a forced frame restart.
			if((value^line_parameter_control_) & value & 0x80) {
				// For now: just force this to be the final line of this mode block.
				// I'm unclear whether I should also reset the horizontal counter
				// (i.e. completely abandon current video phase).
				lines_remaining_ = 0xff;
				should_reload_line_parameters_ = true;
			}
			line_parameter_control_ = value & 0xc0;
		break;
	}
}

uint8_t Nick::read([[maybe_unused]] uint16_t address) {
	return 0xff;
}

Cycles Nick::get_time_until_z80_slot(Cycles after_period) const {
	// Place Z80 accesses as the first six cycles in each sixteen-cycle window.
	// That models video accesses as being the final ten. Which has the net effect
	// of responding to the line parameter table interrupt flag as soon as it's
	// loaded.

	// i.e. 0 -> 0, 1 -> 15 ... 15 -> 1.
	return Cycles(
		15 ^ ((horizontal_counter_ + 15 + after_period.as<int>()) & 15)
	);
}

void Nick::run_for(Cycles duration) {
	constexpr int line_length = 912;

	// TODO: test here for window < 57? Or maybe just nudge up left_/right_margin_ if they
	// exactly equal 57?
#define add_window(x)											\
	window += x;												\
	if(window == left_margin_) is_sync_or_pixels_ = true;		\
	if(window == right_margin_) is_sync_or_pixels_ = false;

	int clocks_remaining = duration.as<int>();
	while(clocks_remaining) {
		// Determine how many cycles are left this line.
		const int clocks_this_line = std::min(clocks_remaining, line_length - horizontal_counter_);

		// Convert that into a [start/current] and end window.
		int window = horizontal_counter_ >> 4;
		const int end_window = (horizontal_counter_ + clocks_this_line) >> 4;

		// Advance the line counters.
		clocks_remaining -= clocks_this_line;
		horizontal_counter_ = (horizontal_counter_ + clocks_this_line) % line_length;

		// Do nothing if a window boundary isn't crossed.
		if(window == end_window) continue;

		// HSYNC is signalled for four windows at the start of the line.
		// I currently believe this happens regardless of Vsync mode.
		//
		// This is also when the non-palette line parameters
		// are loaded, if appropriate.
		if(!window) {
			set_output_type(OutputType::Sync);

			// There's no increment to get to 0, it happens when the horizontal_counter_
			// is reset. So test for active bit effect manually.
			if(!left_margin_) is_sync_or_pixels_ = true;
			if(!right_margin_) is_sync_or_pixels_ = false;
		}

		while(window < 4 && window < end_window) {
			if(should_reload_line_parameters_) {
				switch(window) {
					// First slot: line count, mode and interrupt flag.
					case 0:
						// Byte 0: lines remaining.
						lines_remaining_ = ram_[line_parameter_pointer_];

						// Set the new interrupt line output.
						interrupt_line_ = ram_[line_parameter_pointer_ + 1] & 0x80;

						// Determine the mode and depth, and hence the column size.
						mode_ = Mode((ram_[line_parameter_pointer_ + 1] >> 1)&7);
						bpp_ = 1 << ((ram_[line_parameter_pointer_ + 1] >> 5)&3);
						switch(mode_) {
							default:
							case Mode::Pixel:
								column_size_ = 16 / bpp_;
								line_data_per_column_increments_[0] = 2;
								line_data_per_column_increments_[1] = 0;
							break;

							case Mode::LPixel:
								column_size_ = 8 / bpp_;
								line_data_per_column_increments_[0] = 1;
								line_data_per_column_increments_[1] = 0;
							break;

							case Mode::CH64:
							case Mode::CH128:
							case Mode::CH256:
								column_size_ = 8;
								line_data_per_column_increments_[0] = 1;
								line_data_per_column_increments_[1] = 0;
							break;

							case Mode::Attr:
								column_size_ = 8;
								line_data_per_column_increments_[0] = 1;
								line_data_per_column_increments_[1] = 1;
							break;
						}

						vres_ = ram_[line_parameter_pointer_ + 1] & 0x10;
						reload_line_parameter_pointer_ = ram_[line_parameter_pointer_ + 1] & 0x01;
					break;

					// Second slot: margins and ALT/IND bits.
					case 1:
						// Determine the margins.
						left_margin_ = ram_[line_parameter_pointer_ + 2] & 0x3f;
						right_margin_ = ram_[line_parameter_pointer_ + 3] & 0x3f;

						// Set up the alternative palettes,
						switch(mode_) {
							default:
							break;

							// NB: LSBALT/MSBALT and ALTIND0/ALTIND1 appear to have opposite effects on palette selection.

							case Mode::Pixel:
							case Mode::LPixel: {
								const uint8_t flags = ram_[line_parameter_pointer_ + 2];

								// Use MSBALT and LSBALT to pick the alt_ind_palettes.
								//
								// LSBALT = b6 of params[2], if set => character codes with bit 6 set should use palette indices 4... instead of 0... .
								// MSBALT = b7 of params[2], if set => character codes with bit 7 set should use palette indices 2 and 3.
								two_colour_mask_ = 0xff &~ (((flags&0x80) >> 7) | ((flags&0x40) << 1));

								alt_ind_palettes[0] = palette_;
								alt_ind_palettes[2] = alt_ind_palettes[0] + ((flags & 0x80) ? 2 : 0);

								alt_ind_palettes[1] = alt_ind_palettes[0] + ((flags & 0x40) ? 4 : 0);
								alt_ind_palettes[3] = alt_ind_palettes[2] + ((flags & 0x40) ? 4 : 0);
							} break;

							case Mode::CH64:
							case Mode::CH128:
							case Mode::CH256: {
								const uint8_t flags = ram_[line_parameter_pointer_ + 3];

								// Use ALTIND0 and ALTIND1 to pick the alt_ind_palettes.
								//
								// ALTIND1 = b6 of params[3], if set => character codes with bit 7 set should use palette indices 2 and 3.
								// ALTIND0 = b7 of params[3], if set => character codes with bit 6 set should use palette indices 4... instead of 0... .
								alt_ind_palettes[0] = palette_;
								alt_ind_palettes[2] = alt_ind_palettes[0] + ((flags & 0x40) ? 2 : 0);

								alt_ind_palettes[1] = alt_ind_palettes[0] + ((flags & 0x80) ? 4 : 0);
								alt_ind_palettes[3] = alt_ind_palettes[2] + ((flags & 0x80) ? 4 : 0);
							} break;
						}
					break;

					// Third slot: Line data pointer 1.
					case 2:
						start_line_data_pointer_[0] = ram_[line_parameter_pointer_ + 4];
						start_line_data_pointer_[0] |= ram_[line_parameter_pointer_ + 5] << 8;

						line_data_pointer_[0] = start_line_data_pointer_[0];
					break;

					// Fourth slot: Line data pointer 2.
					case 3:
						start_line_data_pointer_[1] = ram_[line_parameter_pointer_ + 6];
						start_line_data_pointer_[1] |= ram_[line_parameter_pointer_ + 7] << 8;

						line_data_pointer_[1] = start_line_data_pointer_[1];
					break;
				}
			}

			++output_duration_;
			add_window(1);
		}
		if(window == 4) {
			if(mode_ == Mode::Vsync) {
				set_output_type(is_sync_or_pixels_ ? OutputType::Sync : OutputType::Blank);
			} else {
				set_output_type(OutputType::Blank);
			}
		}

		// Deal with vsync mode out here.
		if(mode_ == Mode::Vsync) {
			if(window >= 4) {
				while(window < end_window) {
					// Skip straight to the next event.
					int next_event = end_window;
					if(window < left_margin_) next_event = std::min(next_event, left_margin_);
					if(window < right_margin_) next_event = std::min(next_event, right_margin_);

					output_duration_ += next_event - window;
					add_window(next_event - window);
					set_output_type(is_sync_or_pixels_ ? OutputType::Sync : OutputType::Blank);
				}
			}
		} else {
			// If present then the colour burst is output for the period from
			// the start of window 6 to the end of window 10.
			//
			// The first 8 palette entries also need to be fetched here.
			while(window < 10 && window < end_window) {
				if(window == 6) {
					set_output_type(OutputType::ColourBurst);
				}

				if(should_reload_line_parameters_ && window < 8) {
					const int base = (window - 4) << 1;
					assert(base < 7);
					palette_[base] = mapped_colour(ram_[line_parameter_pointer_ + base + 8]);
					palette_[base + 1] = mapped_colour(ram_[line_parameter_pointer_ + base + 9]);
				}

				++output_duration_;
				add_window(1);
			}

			if(window >= 10) {
				if(window == 10) {
					set_output_type(is_sync_or_pixels_ ? OutputType::Pixels : OutputType::Border);
				}

				while(window < end_window) {
					int next_event = end_window;
					if(window < left_margin_) next_event = std::min(next_event, left_margin_);
					if(window < right_margin_) next_event = std::min(next_event, right_margin_);

					if(is_sync_or_pixels_) {

#define DispatchBpp(func) \
	switch(bpp_) {	\
		default:	\
		case 1: func(1)(pixel_pointer_, output_duration);	break;	\
		case 2: func(2)(pixel_pointer_, output_duration);	break;	\
		case 4: func(4)(pixel_pointer_, output_duration);	break;	\
		case 8: func(8)(pixel_pointer_, output_duration);	break;	\
	}

#define pixel(x) output_pixel<x, false>
#define lpixel(x) output_pixel<x, true>
#define ch256(x) output_character<x, 8>
#define ch128(x) output_character<x, 7>
#define ch64(x) output_character<x, 6>
#define attr(x) output_attributed<x>

						int columns_remaining = next_event - window;
						while(columns_remaining) {
							if(!pixel_pointer_) {
								if(output_duration_) {
									set_output_type(OutputType::Pixels, true);
								}
								pixel_pointer_ = allocated_pointer_ = reinterpret_cast<uint16_t *>(crt_.begin_data(allocation_size));
							}

							if(allocated_pointer_) {
								const int output_duration = std::min(columns_remaining, int(allocated_pointer_ + allocation_size - pixel_pointer_) / column_size_);

								switch(mode_) {
									default:
									case Mode::Pixel:	DispatchBpp(pixel);		break;
									case Mode::LPixel:	DispatchBpp(lpixel);	break;
									case Mode::CH256:	DispatchBpp(ch256);		break;
									case Mode::CH128:	DispatchBpp(ch128);		break;
									case Mode::CH64:	DispatchBpp(ch64);		break;
									case Mode::Attr:	DispatchBpp(attr);		break;
								}

								pixel_pointer_ += output_duration * column_size_;
								output_duration_ += output_duration;
								if(pixel_pointer_ - allocated_pointer_ == allocation_size) {
									set_output_type(OutputType::Pixels, true);
								}
								columns_remaining -= output_duration;
							} else {
								// Ensure line data pointers are advanced as if there hadn't been back pressure on
								// pixel rendering.
								line_data_pointer_[0] += columns_remaining * line_data_per_column_increments_[0];
								line_data_pointer_[1] += columns_remaining * line_data_per_column_increments_[1];

								output_duration_ += columns_remaining;
								columns_remaining = 0;
							}
						}

#undef attr
#undef ch64
#undef ch128
#undef ch256
#undef pixel
#undef lpixel
#undef DispatchBpp
					} else {
						output_duration_ += next_event - window;
					}

					add_window(next_event - window);
					set_output_type(is_sync_or_pixels_ ? OutputType::Pixels : OutputType::Border);
				}
			}
		}

		// Check for end of line.
		if(!horizontal_counter_) {
			++lines_remaining_;
			if(!lines_remaining_) {
				should_reload_line_parameters_ = true;

				// Check for end-of-frame.
				if(reload_line_parameter_pointer_) {
					line_parameter_pointer_ = line_parameter_base_;
				} else {
					line_parameter_pointer_ += 16;
				}
			} else {
				should_reload_line_parameters_ = false;
			}

			// Deal with VRES and other address reloading, dependant upon mode.
			switch(mode_) {
				default: break;
				case Mode::CH64:
				case Mode::CH128:
				case Mode::CH256:
					line_data_pointer_[0] = start_line_data_pointer_[0];
					++line_data_pointer_[1];
				break;

				case Mode::Attr:
					// Reload the attribute address if VRES is set.
					if(vres_) {
						line_data_pointer_[0] = start_line_data_pointer_[0];
					}
				break;

				case Mode::Pixel:
				case Mode::LPixel:
					// If VRES is clear, reload the pixel address.
					if(!vres_) {
						line_data_pointer_[0] = start_line_data_pointer_[0];
					}
				break;
			}
		}
	}

#undef add_window

}

void Nick::set_output_type(OutputType type, bool force_flush) {
	if(type == output_type_ && !force_flush) {
		return;
	}
	if(output_duration_) {
		switch(output_type_) {
			case OutputType::Border: {
				uint16_t *const colour_pointer = reinterpret_cast<uint16_t *>(crt_.begin_data(1));
				if(colour_pointer) *colour_pointer = border_colour_;
				crt_.output_level(output_duration_*16);
			} break;

			case OutputType::Pixels: {
				crt_.output_data(output_duration_*16, size_t(output_duration_*column_size_));
				pixel_pointer_ = nullptr;
				allocated_pointer_ = nullptr;
			} break;

			case OutputType::Sync:			crt_.output_sync(output_duration_*16);				break;
			case OutputType::Blank:			crt_.output_blank(output_duration_*16);				break;
			case OutputType::ColourBurst:	crt_.output_colour_burst(output_duration_*16, 0);	break;
		}
	}

	output_duration_ = 0;
	output_type_ = type;
}

// MARK: - Sequence points.

Cycles Nick::get_next_sequence_point() const {
	// TODO: the below is incorrect; unit test and correct.
	// Changing to e.g. Cycles(1) reveals the relevant discrepancy.
//	return Cycles(1);

	constexpr int load_point = 2*16;

	// Any mode line may cause a change in the interrupt output, so as a first blush
	// just always report the time until the end of the mode line.
	if(lines_remaining_ || horizontal_counter_ >= load_point) {
		return Cycles(load_point + (912 - horizontal_counter_) + (0xff - lines_remaining_) * 912);
	} else {
		return Cycles(load_point - horizontal_counter_);
	}
}

// MARK: - CRT passthroughs.

void Nick::set_scan_target(Outputs::Display::ScanTarget *scan_target) {
	crt_.set_scan_target(scan_target);
}

Outputs::Display::ScanStatus Nick::get_scaled_scan_status() const {
	return crt_.get_scaled_scan_status();
}

// MARK: - Specific pixel outputters.

#define output1bpp(x)	\
	target[0] = palette[(x & 0x80) >> 7];	\
	target[1] = palette[(x & 0x40) >> 6];	\
	target[2] = palette[(x & 0x20) >> 5];	\
	target[3] = palette[(x & 0x10) >> 4];	\
	target[4] = palette[(x & 0x08) >> 3];	\
	target[5] = palette[(x & 0x04) >> 2];	\
	target[6] = palette[(x & 0x02) >> 1];	\
	target[7] = palette[(x & 0x01) >> 0];	\
	target += 8

#define output2bpp(x)	\
	target[0] = palette_[((x & 0x80) >> 7) | ((x & 0x08) >> 2)];	\
	target[1] = palette_[((x & 0x40) >> 6) | ((x & 0x04) >> 1)];	\
	target[2] = palette_[((x & 0x20) >> 5) | ((x & 0x02) >> 0)];	\
	target[3] = palette_[((x & 0x10) >> 4) | ((x & 0x01) << 1)];	\
	target += 4

#define output4bpp(x)	\
	target[0] = palette_[((x & 0x02) << 2) | ((x & 0x20) >> 3) | ((x & 0x08) >> 2) | ((x & 0x80) >> 7)];	\
	target[1] = palette_[((x & 0x01) << 3) | ((x & 0x10) >> 2) | ((x & 0x04) >> 1) | ((x & 0x40) >> 6)];	\
	target += 2

#define output8bpp(x)	\
	target[0] = mapped_colour(x);	\
	++target

template <int bpp, bool is_lpixel> void Nick::output_pixel(uint16_t *target, int columns) {
	static_assert(bpp == 1 || bpp == 2 || bpp == 4 || bpp == 8);

	for(int c = 0; c < columns; c++) {
		uint8_t pixels[2] = { ram_[line_data_pointer_[0]], ram_[(line_data_pointer_[0]+1) & 0xffff] };
		line_data_pointer_[0] += is_lpixel ? 1 : 2;

		switch(bpp) {
			default:
			case 1: {
				const uint16_t *palette = alt_ind_palettes[((pixels[0] >> 6) & 0x02) | (pixels[0]&1)];
				pixels[0] &= two_colour_mask_;
				output1bpp(pixels[0]);

				if constexpr (!is_lpixel) {
					palette = alt_ind_palettes[((pixels[1] >> 6) & 0x02) | (pixels[1]&1)];
					pixels[1] &= two_colour_mask_;
					output1bpp(pixels[1]);
				}
			} break;

			case 2:
				output2bpp(pixels[0]);
				if constexpr (!is_lpixel) {
					output2bpp(pixels[1]);
				}
			break;

			case 4:
				output4bpp(pixels[0]);
				if constexpr (!is_lpixel) {
					output4bpp(pixels[1]);
				}
			break;

			case 8:
				output8bpp(pixels[0]);
				if constexpr (!is_lpixel) {
					output8bpp(pixels[1]);
				}
			break;
		}
	}
}

template <int bpp, int index_bits> void Nick::output_character(uint16_t *target, int columns) {
	static_assert(bpp == 1 || bpp == 2 || bpp == 4 || bpp == 8);

	for(int c = 0; c < columns; c++) {
		const uint8_t character = ram_[line_data_pointer_[0]];
		++line_data_pointer_[0];

		const uint8_t pixels = ram_[(
			(line_data_pointer_[1] << index_bits) +
			(character & ((1 << index_bits) - 1))
		) & 0xffff];

		// TODO: below looks repetitious of the above, but I've yet to factor in
		// ALTINDs and [M/L]SBALTs, so I'll correct for factoring when I've done that.

		switch(bpp) {
			default:
				assert(false);
			break;

			case 1: {
				// This applies ALTIND0 and ALTIND1.
				const uint16_t *palette = alt_ind_palettes[character >> 6];
				output1bpp(pixels);
			} break;

			case 2:		output2bpp(pixels);		break;
			case 4:		output4bpp(pixels);		break;
			case 8:		output8bpp(pixels);		break;
		}
	}
}

template <int bpp> void Nick::output_attributed(uint16_t *target, int columns) {
	static_assert(bpp == 1 || bpp == 2 || bpp == 4 || bpp == 8);

	for(int c = 0; c < columns; c++) {
		const uint8_t pixels = ram_[line_data_pointer_[1]];
		const uint8_t attributes = ram_[line_data_pointer_[0]];

		++line_data_pointer_[0];
		++line_data_pointer_[1];

		const uint16_t palette[2] = {
			palette_[attributes >> 4], palette_[attributes & 0x0f]
		};
		output1bpp(pixels);
	}
}

#undef output1bpp
#undef output2bpp
#undef output4bpp
#undef output8bpp
