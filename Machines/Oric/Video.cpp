//
//  Video.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 12/10/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#include "Video.hpp"

using namespace Oric;

namespace {
	const unsigned int PAL50VSyncStartPosition = 256*64;
	const unsigned int PAL60VSyncStartPosition = 234*64;
	const unsigned int PAL50VSyncEndPosition = 259*64;
	const unsigned int PAL60VSyncEndPosition = 238*64;
	const unsigned int PAL50Period = 312*64;
	const unsigned int PAL60Period = 262*64;
}

VideoOutput::VideoOutput(uint8_t *memory) :
		ram_(memory),
		crt_(new Outputs::CRT::CRT(64*6, 6, Outputs::CRT::DisplayType::PAL50, 2)),
		v_sync_start_position_(PAL50VSyncStartPosition), v_sync_end_position_(PAL50VSyncEndPosition),
		counter_period_(PAL50Period) {
	crt_->set_rgb_sampling_function(
		"vec3 rgb_sample(usampler2D sampler, vec2 coordinate, vec2 icoordinate)"
		"{"
			"uint texValue = texture(sampler, coordinate).r;"
			"return vec3( uvec3(texValue) & uvec3(4u, 2u, 1u));"
		"}");
	crt_->set_composite_sampling_function(
		"float composite_sample(usampler2D sampler, vec2 coordinate, vec2 icoordinate, float phase, float amplitude)"
		"{"
			"uint texValue = uint(dot(texture(sampler, coordinate).rg, uvec2(1, 256)));"
			"uint iPhase = uint((phase + 3.141592654 + 0.39269908175) * 2.0 / 3.141592654) & 3u;"
			"texValue = (texValue >> (4u*(3u - iPhase))) & 15u;"
			"return (float(texValue) - 4.0) / 20.0;"
		"}"
	);
	crt_->set_composite_function_type(Outputs::CRT::CRT::CompositeSourceType::DiscreteFourSamplesPerCycle, 0.0f);

	set_video_signal(Outputs::CRT::VideoSignal::Composite);
	crt_->set_visible_area(crt_->get_rect_for_area(53, 224, 16 * 6, 40 * 6, 4.0f / 3.0f));
}

void VideoOutput::set_video_signal(Outputs::CRT::VideoSignal video_signal) {
	video_signal_ = video_signal;
	crt_->set_video_signal(video_signal);
}

void VideoOutput::set_colour_rom(const std::vector<uint8_t> &rom) {
	for(std::size_t c = 0; c < 8; c++) {
		std::size_t index = (c << 2);
		uint16_t rom_value = static_cast<uint16_t>((static_cast<uint16_t>(rom[index]) << 8) | static_cast<uint16_t>(rom[index+1]));
		rom_value = (rom_value & 0xff00) | ((rom_value >> 4)&0x000f) | ((rom_value << 4)&0x00f0);
		colour_forms_[c] = rom_value;
	}

	// check for big endianness and byte swap if required
	uint16_t test_value = 0x0001;
	if(*reinterpret_cast<uint8_t *>(&test_value) != 0x01) {
		for(std::size_t c = 0; c < 8; c++) {
			colour_forms_[c] = static_cast<uint16_t>((colour_forms_[c] >> 8) | (colour_forms_[c] << 8));
		}
	}
}

Outputs::CRT::CRT *VideoOutput::get_crt() {
	return crt_.get();
}

void VideoOutput::run_for(const Cycles cycles) {
	// Vertical: 0–39: pixels; otherwise blank; 48–53 sync, 54–56 colour burst
	// Horizontal: 0–223: pixels; otherwise blank; 256–259 sync

#define clamp(action)	\
	if(cycles_run_for <= number_of_cycles) { action; } else cycles_run_for = number_of_cycles;

	int number_of_cycles = cycles.as_int();
	while(number_of_cycles) {
		int h_counter = counter_ & 63;
		int cycles_run_for = 0;

		if(counter_ >= v_sync_start_position_ && counter_ < v_sync_end_position_) {
			// this is a sync line
			cycles_run_for = v_sync_end_position_ - counter_;
			clamp(crt_->output_sync(static_cast<unsigned int>(v_sync_end_position_ - v_sync_start_position_) * 6));
		} else if(counter_ < 224*64 && h_counter < 40) {
			// this is a pixel line
			if(!h_counter) {
				ink_ = 0x7;
				paper_ = 0x0;
				use_alternative_character_set_ = use_double_height_characters_ = blink_text_ = false;
				set_character_set_base_address();
				pixel_target_ = reinterpret_cast<uint16_t *>(crt_->allocate_write_area(240));

				if(!counter_) {
					frame_counter_++;

					v_sync_start_position_ = next_frame_is_sixty_hertz_ ? PAL60VSyncStartPosition : PAL50VSyncStartPosition;
					v_sync_end_position_ = next_frame_is_sixty_hertz_ ? PAL60VSyncEndPosition : PAL50VSyncEndPosition;
					counter_period_ = next_frame_is_sixty_hertz_ ? PAL60Period : PAL50Period;
				}
			}

			cycles_run_for = std::min(40 - h_counter, number_of_cycles);
			int columns = cycles_run_for;
			int pixel_base_address = 0xa000 + (counter_ >> 6) * 40;
			int character_base_address = 0xbb80 + (counter_ >> 9) * 40;
			uint8_t blink_mask = (blink_text_ && (frame_counter_&32)) ? 0x00 : 0xff;

			while(columns--) {
				uint8_t pixels, control_byte;

				if(is_graphics_mode_ && counter_ < 200*64) {
					control_byte = pixels = ram_[pixel_base_address + h_counter];
				} else {
					int address = character_base_address + h_counter;
					control_byte = ram_[address];
					int line = use_double_height_characters_ ? ((counter_ >> 7) & 7) : ((counter_ >> 6) & 7);
					pixels = ram_[character_set_base_address_ + (control_byte&127) * 8 + line];
				}

				uint8_t inverse_mask = (control_byte & 0x80) ? 0x7 : 0x0;
				pixels &= blink_mask;

				if(control_byte & 0x60) {
					if(pixel_target_) {
						uint16_t colours[2];
						if(video_signal_ == Outputs::CRT::VideoSignal::RGB) {
							colours[0] = static_cast<uint8_t>(paper_ ^ inverse_mask);
							colours[1] = static_cast<uint8_t>(ink_ ^ inverse_mask);
						} else {
							colours[0] = colour_forms_[paper_ ^ inverse_mask];
							colours[1] = colour_forms_[ink_ ^ inverse_mask];
						}
						pixel_target_[0] = colours[(pixels >> 5)&1];
						pixel_target_[1] = colours[(pixels >> 4)&1];
						pixel_target_[2] = colours[(pixels >> 3)&1];
						pixel_target_[3] = colours[(pixels >> 2)&1];
						pixel_target_[4] = colours[(pixels >> 1)&1];
						pixel_target_[5] = colours[(pixels >> 0)&1];
					}
				} else {
					switch(control_byte & 0x1f) {
						case 0x00:		ink_ = 0x0;	break;
						case 0x01:		ink_ = 0x4;	break;
						case 0x02:		ink_ = 0x2;	break;
						case 0x03:		ink_ = 0x6;	break;
						case 0x04:		ink_ = 0x1;	break;
						case 0x05:		ink_ = 0x5;	break;
						case 0x06:		ink_ = 0x3;	break;
						case 0x07:		ink_ = 0x7;	break;

						case 0x08:	case 0x09:	case 0x0a: case 0x0b:
						case 0x0c:	case 0x0d:	case 0x0e: case 0x0f:
							use_alternative_character_set_ = (control_byte&1);
							use_double_height_characters_ = (control_byte&2);
							blink_text_ = (control_byte&4);
							set_character_set_base_address();
						break;

						case 0x10:		paper_ = 0x0;	break;
						case 0x11:		paper_ = 0x4;	break;
						case 0x12:		paper_ = 0x2;	break;
						case 0x13:		paper_ = 0x6;	break;
						case 0x14:		paper_ = 0x1;	break;
						case 0x15:		paper_ = 0x5;	break;
						case 0x16:		paper_ = 0x3;	break;
						case 0x17:		paper_ = 0x7;	break;

						case 0x18: case 0x19: case 0x1a: case 0x1b:
						case 0x1c: case 0x1d: case 0x1e: case 0x1f:
							is_graphics_mode_ = (control_byte & 4);
							next_frame_is_sixty_hertz_ = !(control_byte & 2);
						break;

						default: break;
					}
					if(pixel_target_) {
						pixel_target_[0] = pixel_target_[1] =
						pixel_target_[2] = pixel_target_[3] =
						pixel_target_[4] = pixel_target_[5] =
							(video_signal_ == Outputs::CRT::VideoSignal::RGB) ? paper_ ^ inverse_mask : colour_forms_[paper_ ^ inverse_mask];
					}
				}
				if(pixel_target_) pixel_target_ += 6;
				h_counter++;
			}

			if(h_counter == 40) {
				crt_->output_data(40 * 6, 1);
			}
		} else {
			// this is a blank line (or the equivalent part of a pixel line)
			if(h_counter < 48) {
				cycles_run_for = 48 - h_counter;
				clamp(
					int period = (counter_ < 224*64) ? 8 : 48;
					crt_->output_blank(static_cast<unsigned int>(period) * 6);
				);
			} else if(h_counter < 54) {
				cycles_run_for = 54 - h_counter;
				clamp(crt_->output_sync(6 * 6));
			} else if(h_counter < 56) {
				cycles_run_for = 56 - h_counter;
				clamp(crt_->output_default_colour_burst(2 * 6));
			} else {
				cycles_run_for = 64 - h_counter;
				clamp(crt_->output_blank(8 * 6));
			}
		}

		counter_ = (counter_ + cycles_run_for)%counter_period_;
		number_of_cycles -= cycles_run_for;
	}
}

void VideoOutput::set_character_set_base_address() {
	if(is_graphics_mode_) character_set_base_address_ = use_alternative_character_set_ ? 0x9c00 : 0x9800;
	else character_set_base_address_ = use_alternative_character_set_ ? 0xb800 : 0xb400;
}
