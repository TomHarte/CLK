//
//  Video.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 10/12/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#include "Video.hpp"

using namespace Electron;

#define graphics_line(v)	((((v) >> 7) - first_graphics_line + field_divider_line) % field_divider_line)
#define graphics_column(v)	((((v) & 127) - first_graphics_cycle + 128) & 127)

namespace {
	static const int cycles_per_line = 128;
	static const int lines_per_frame = 625;
	static const int cycles_per_frame = lines_per_frame * cycles_per_line;
	static const int crt_cycles_multiplier = 8;
	static const int crt_cycles_per_line = crt_cycles_multiplier * cycles_per_line;

	static const int field_divider_line = 312;	// i.e. the line, simultaneous with which, the first field's sync ends. So if
														// the first line with pixels in field 1 is the 20th in the frame, the first line
														// with pixels in field 2 will be 20+field_divider_line
	static const int first_graphics_line = 31;
	static const int first_graphics_cycle = 33;

	static const int display_end_interrupt_line = 256;

	static const int real_time_clock_interrupt_1 = 16704;
	static const int real_time_clock_interrupt_2 = 56704;
	static const int display_end_interrupt_1 = (first_graphics_line + display_end_interrupt_line)*cycles_per_line;
	static const int display_end_interrupt_2 = (first_graphics_line + field_divider_line + display_end_interrupt_line)*cycles_per_line;
}

VideoOutput::VideoOutput(uint8_t *memory) :
	ram_(memory),
	current_pixel_line_(-1),
	output_position_(0),
	screen_mode_(6)
{
	memset(palette_, 0xf, sizeof(palette_));

	crt_.reset(new Outputs::CRT::CRT(crt_cycles_per_line, 8, Outputs::CRT::DisplayType::PAL50, 1));
	crt_->set_rgb_sampling_function(
		"vec3 rgb_sample(usampler2D sampler, vec2 coordinate, vec2 icoordinate)"
		"{"
			"uint texValue = texture(sampler, coordinate).r;"
			"texValue >>= 4 - (int(icoordinate.x * 8) & 4);"
			"return vec3( uvec3(texValue) & uvec3(4u, 2u, 1u));"
		"}");
	// TODO: as implied below, I've introduced a clock's latency into the graphics pipeline somehow. Investigate.
	crt_->set_visible_area(crt_->get_rect_for_area(first_graphics_line - 3, 256, (first_graphics_cycle+1) * crt_cycles_multiplier, 80 * crt_cycles_multiplier, 4.0f / 3.0f));
}

std::shared_ptr<Outputs::CRT::CRT> VideoOutput::get_crt()
{
	return crt_;
}

void VideoOutput::start_pixel_line()
{
	current_pixel_line_ = (current_pixel_line_+1)&255;
	if(!current_pixel_line_)
	{
		start_line_address_ = start_screen_address_;
		current_character_row_ = 0;
		is_blank_line_ = false;
	}
	else
	{
		bool mode_has_blank_lines = (screen_mode_ == 6) || (screen_mode_ == 3);
		is_blank_line_ = (mode_has_blank_lines && ((current_character_row_ > 7 && current_character_row_ < 10) || (current_pixel_line_ > 249)));

		if(!is_blank_line_)
		{
			start_line_address_++;

			if(current_character_row_ > 7)
			{
				start_line_address_ += ((screen_mode_ < 4) ? 80 : 40) * 8 - 8;
				current_character_row_ = 0;
			}
		}
	}
	current_screen_address_ = start_line_address_;
	current_pixel_column_ = 0;
	initial_output_target_ = current_output_target_ = nullptr;
}

void VideoOutput::end_pixel_line()
{
	if(current_output_target_) crt_->output_data((unsigned int)((current_output_target_ - initial_output_target_) * current_output_divider_), current_output_divider_);
	current_character_row_++;
}

void VideoOutput::output_pixels(unsigned int number_of_cycles)
{
	if(!number_of_cycles) return;

	if(is_blank_line_)
	{
		crt_->output_blank(number_of_cycles * crt_cycles_multiplier);
	}
	else
	{
		unsigned int divider = 0;
		switch(screen_mode_)
		{
			case 0: case 3: divider = 2; break;
			case 1: case 4: case 6: divider = 4; break;
			case 2: case 5: divider = 8; break;
		}

		if(!initial_output_target_ || divider != current_output_divider_)
		{
			if(current_output_target_) crt_->output_data((unsigned int)((current_output_target_ - initial_output_target_) * current_output_divider_), current_output_divider_);
			current_output_divider_ = divider;
			initial_output_target_ = current_output_target_ = crt_->allocate_write_area(640 / current_output_divider_);
		}

#define get_pixel()	\
				if(current_screen_address_&32768)\
				{\
					current_screen_address_ = (screen_mode_base_address_ + current_screen_address_)&32767;\
				}\
				last_pixel_byte_ = ram_[current_screen_address_];\
				current_screen_address_ = current_screen_address_+8

		switch(screen_mode_)
		{
			case 0: case 3:
				if(initial_output_target_)
				{
					while(number_of_cycles--)
					{
						get_pixel();
						*(uint32_t *)current_output_target_ = palette_tables_.eighty1bpp[last_pixel_byte_];
						current_output_target_ += 4;
						current_pixel_column_++;
					}
				} else current_output_target_ += 4*number_of_cycles;
			break;

			case 1:
				if(initial_output_target_)
				{
					while(number_of_cycles--)
					{
						get_pixel();
						*(uint16_t *)current_output_target_ = palette_tables_.eighty2bpp[last_pixel_byte_];
						current_output_target_ += 2;
						current_pixel_column_++;
					}
				} else current_output_target_ += 2*number_of_cycles;
			break;

			case 2:
				if(initial_output_target_)
				{
					while(number_of_cycles--)
					{
						get_pixel();
						*current_output_target_ = palette_tables_.eighty4bpp[last_pixel_byte_];
						current_output_target_ += 1;
						current_pixel_column_++;
					}
				} else current_output_target_ += number_of_cycles;
			break;

			case 4: case 6:
				if(initial_output_target_)
				{
					if(current_pixel_column_&1)
					{
						last_pixel_byte_ <<= 4;
						*(uint16_t *)current_output_target_ = palette_tables_.forty1bpp[last_pixel_byte_];
						current_output_target_ += 2;

						number_of_cycles--;
						current_pixel_column_++;
					}
					while(number_of_cycles > 1)
					{
						get_pixel();
						*(uint16_t *)current_output_target_ = palette_tables_.forty1bpp[last_pixel_byte_];
						current_output_target_ += 2;

						last_pixel_byte_ <<= 4;
						*(uint16_t *)current_output_target_ = palette_tables_.forty1bpp[last_pixel_byte_];
						current_output_target_ += 2;

						number_of_cycles -= 2;
						current_pixel_column_+=2;
					}
					if(number_of_cycles)
					{
						get_pixel();
						*(uint16_t *)current_output_target_ = palette_tables_.forty1bpp[last_pixel_byte_];
						current_output_target_ += 2;
						current_pixel_column_++;
					}
				} else current_output_target_ += 2 * number_of_cycles;
			break;

			case 5:
				if(initial_output_target_)
				{
					if(current_pixel_column_&1)
					{
						last_pixel_byte_ <<= 2;
						*current_output_target_ = palette_tables_.forty2bpp[last_pixel_byte_];
						current_output_target_ += 1;

						number_of_cycles--;
						current_pixel_column_++;
					}
					while(number_of_cycles > 1)
					{
						get_pixel();
						*current_output_target_ = palette_tables_.forty2bpp[last_pixel_byte_];
						current_output_target_ += 1;

						last_pixel_byte_ <<= 2;
						*current_output_target_ = palette_tables_.forty2bpp[last_pixel_byte_];
						current_output_target_ += 1;

						number_of_cycles -= 2;
						current_pixel_column_+=2;
					}
					if(number_of_cycles)
					{
						get_pixel();
						*current_output_target_ = palette_tables_.forty2bpp[last_pixel_byte_];
						current_output_target_ += 1;
						current_pixel_column_++;
					}
				} else current_output_target_ += number_of_cycles;
			break;
		}

#undef get_pixel
	}
}

void VideoOutput::run_for_inner_frame_cycles(int number_of_cycles)
{
	int target_output_position = output_position_ + number_of_cycles;
	int final_line = target_output_position >> 7;

	while(output_position_ < target_output_position)
	{
		int line = output_position_ >> 7;

		// Priority one: sync.
		// ===================

		// full sync lines are 0, 1, field_divider_line+1 and field_divider_line+2
		if(line == 0 || line == 1 || line == field_divider_line+1 || line == field_divider_line+2)
		{
			// wait for the line to complete before signalling
			if(final_line == line) return;
			crt_->output_sync(128 * crt_cycles_multiplier);
			output_position_ += 128;
			continue;
		}

		// line 2 is a left-sync line
		if(line == 2)
		{
			// wait for the line to complete before signalling
			if(final_line == line) return;
			crt_->output_sync(64 * crt_cycles_multiplier);
			crt_->output_blank(64 * crt_cycles_multiplier);
			output_position_ += 128;
			continue;
		}

		// line field_divider_line is a right-sync line
		if(line == field_divider_line)
		{
			// wait for the line to complete before signalling
			if(final_line == line) return;
			crt_->output_sync(9 * crt_cycles_multiplier);
			crt_->output_blank(55 * crt_cycles_multiplier);
			crt_->output_sync(64 * crt_cycles_multiplier);
			output_position_ += 128;
			continue;
		}

		// Priority two: blank lines.
		// ==========================
		//
		// Given that it is not a sync line, this is a blank line if it is less than first_graphics_line, or greater
		// than first_graphics_line+255 and less than first_graphics_line+field_divider_line, or greater than
		// first_graphics_line+field_divider_line+255 (TODO: or this is Mode 3 or 6 and this should be blank)
		if(
			line < first_graphics_line ||
			(line > first_graphics_line+255 && line < first_graphics_line+field_divider_line) ||
			line > first_graphics_line+field_divider_line+255)
		{
			if(final_line == line) return;
			crt_->output_sync(9 * crt_cycles_multiplier);
			crt_->output_blank(119 * crt_cycles_multiplier);
			output_position_ += 128;
			continue;
		}

		// Final possibility: this is a pixel line.
		// ========================================

		// determine how far we're going from left to right
		int this_cycle = output_position_&127;
		int final_cycle = target_output_position&127;
		if(final_line > line)
		{
			final_cycle = 128;
		}

		// output format is:
		// 9 cycles: sync
		// ... to 24 cycles: colour burst
		// ... to first_graphics_cycle: blank
		// ... for 80 cycles: pixels
		// ... until end of line: blank
		while(this_cycle < final_cycle)
		{
			if(this_cycle < 9)
			{
				if(final_cycle < 9) return;
				crt_->output_sync(9 * crt_cycles_multiplier);
				output_position_ += 9;
				this_cycle = 9;
			}

			if(this_cycle < 24)
			{
				if(final_cycle < 24) return;
				crt_->output_default_colour_burst((24-9) * crt_cycles_multiplier);
				output_position_ += 24-9;
				this_cycle = 24;
				// TODO: phase shouldn't be zero on every line
			}

			if(this_cycle < first_graphics_cycle)
			{
				if(final_cycle < first_graphics_cycle) return;
				crt_->output_blank((first_graphics_cycle - 24) * crt_cycles_multiplier);
				output_position_ += first_graphics_cycle - 24;
				this_cycle = first_graphics_cycle;
				start_pixel_line();
			}

			if(this_cycle < first_graphics_cycle + 80)
			{
				unsigned int length_to_output = std::min(final_cycle, (first_graphics_cycle + 80)) - this_cycle;
				output_pixels(length_to_output);
				output_position_ += length_to_output;
				this_cycle += length_to_output;
			}

			if(this_cycle >= first_graphics_cycle + 80)
			{
				if(final_cycle < 128) return;
				end_pixel_line();
				crt_->output_blank((128 - (first_graphics_cycle + 80)) * crt_cycles_multiplier);
				output_position_ += 128 - (first_graphics_cycle + 80);
				this_cycle = 128;
			}
		}
	}
}

void VideoOutput::run_for_cycles(int number_of_cycles)
{
	/*

		Odd field:					Even field:

		|--S--|						   -S-|
		|--S--|						|--S--|
		|-S-B-|	= 3					|--S--| = 2.5
		|--B--|						|--B--|
		|--P--|						|--P--|
		|--B--| = 312				|--B--| = 312.5
		|-B-

	*/
	int cycles_at_end = unused_cycles_ + output_position_ + number_of_cycles;
	unused_cycles_ = 0;

	int number_of_frames = 1 + (cycles_at_end / cycles_per_frame);
	while(number_of_frames--)
	{
		int frame_target = number_of_frames ? cycles_per_frame : (cycles_at_end % cycles_per_frame);
		run_for_inner_frame_cycles(frame_target - output_position_);
//		unused_cycles_ += (frame_final - output_position_);
//		if(unused_cycles_)
//		{
//		}

		output_position_ %= cycles_per_frame;
	}
}

void VideoOutput::set_register(int address, uint8_t value)
{
	switch(address & 0xf)
	{
		case 0x02:
			start_screen_address_ = (start_screen_address_ & 0xfe00) | (uint16_t)((value & 0xe0) << 1);
			if(!start_screen_address_) start_screen_address_ |= 0x8000;
		break;
		case 0x03:
			start_screen_address_ = (start_screen_address_ & 0x01ff) | (uint16_t)((value & 0x3f) << 9);
			if(!start_screen_address_) start_screen_address_ |= 0x8000;
		break;
		case 0x07:
		{
			// update screen mode
			uint8_t new_screen_mode = (value >> 3)&7;
			if(new_screen_mode == 7) new_screen_mode = 4;
			if(new_screen_mode != screen_mode_)
			{
				screen_mode_ = new_screen_mode;
				switch(screen_mode_)
				{
					case 0: case 1: case 2: screen_mode_base_address_ = 0x3000; break;
					case 3: screen_mode_base_address_ = 0x4000; break;
					case 4: case 5: screen_mode_base_address_ = 0x5800; break;
					case 6: screen_mode_base_address_ = 0x6000; break;
				}
			}
		}
		break;
		case 0x08: case 0x09: case 0x0a: case 0x0b:
		case 0x0c: case 0x0d: case 0x0e: case 0x0f:
		{
			static const int registers[4][4] = {
				{10, 8, 2, 0},
				{14, 12, 6, 4},
				{15, 13, 7, 5},
				{11, 9, 3, 1},
			};
			const int index = (address >> 1)&3;
			const uint8_t colour = ~value;
			if(address&1)
			{
				palette_[registers[index][0]]	= (palette_[registers[index][0]]&3)	| ((colour >> 1)&4);
				palette_[registers[index][1]]	= (palette_[registers[index][1]]&3)	| ((colour >> 0)&4);
				palette_[registers[index][2]]	= (palette_[registers[index][2]]&3)	| ((colour << 1)&4);
				palette_[registers[index][3]]	= (palette_[registers[index][3]]&3)	| ((colour << 2)&4);

				palette_[registers[index][2]]	= (palette_[registers[index][2]]&5)	| ((colour >> 4)&2);
				palette_[registers[index][3]]	= (palette_[registers[index][3]]&5)	| ((colour >> 3)&2);
			}
			else
			{
				palette_[registers[index][0]]	= (palette_[registers[index][0]]&6)	| ((colour >> 7)&1);
				palette_[registers[index][1]]	= (palette_[registers[index][1]]&6)	| ((colour >> 6)&1);
				palette_[registers[index][2]]	= (palette_[registers[index][2]]&6)	| ((colour >> 5)&1);
				palette_[registers[index][3]]	= (palette_[registers[index][3]]&6)	| ((colour >> 4)&1);

				palette_[registers[index][0]]	= (palette_[registers[index][0]]&5)	| ((colour >> 2)&2);
				palette_[registers[index][1]]	= (palette_[registers[index][1]]&5)	| ((colour >> 1)&2);
			}

			// regenerate all palette tables for now
#define pack(a, b) (uint8_t)((a << 4) | (b))
			for(int byte = 0; byte < 256; byte++)
			{
				uint8_t *target = (uint8_t *)&palette_tables_.forty1bpp[byte];
				target[0] = pack(palette_[(byte&0x80) >> 4], palette_[(byte&0x40) >> 3]);
				target[1] = pack(palette_[(byte&0x20) >> 2], palette_[(byte&0x10) >> 1]);

				target = (uint8_t *)&palette_tables_.eighty2bpp[byte];
				target[0] = pack(palette_[((byte&0x80) >> 4) | ((byte&0x08) >> 2)], palette_[((byte&0x40) >> 3) | ((byte&0x04) >> 1)]);
				target[1] = pack(palette_[((byte&0x20) >> 2) | ((byte&0x02) >> 0)], palette_[((byte&0x10) >> 1) | ((byte&0x01) << 1)]);

				target = (uint8_t *)&palette_tables_.eighty1bpp[byte];
				target[0] = pack(palette_[(byte&0x80) >> 4], palette_[(byte&0x40) >> 3]);
				target[1] = pack(palette_[(byte&0x20) >> 2], palette_[(byte&0x10) >> 1]);
				target[2] = pack(palette_[(byte&0x08) >> 0], palette_[(byte&0x04) << 1]);
				target[3] = pack(palette_[(byte&0x02) << 2], palette_[(byte&0x01) << 3]);

				palette_tables_.forty2bpp[byte] = pack(palette_[((byte&0x80) >> 4) | ((byte&0x08) >> 2)], palette_[((byte&0x40) >> 3) | ((byte&0x04) >> 1)]);
				palette_tables_.eighty4bpp[byte] = pack(	palette_[((byte&0x80) >> 4) | ((byte&0x20) >> 3) | ((byte&0x08) >> 2) | ((byte&0x02) >> 1)],
														palette_[((byte&0x40) >> 3) | ((byte&0x10) >> 2) | ((byte&0x04) >> 1) | ((byte&0x01) >> 0)]);
			}
#undef pack
		}
		break;
	}
}

#pragma mark - Interrupts

//int VideoOutput::get_cycles_until_next_interrupt()
//{
//	const int end_of_field =
//	if(frame_cycles_ < (256 + first_graphics_line) << 7))

//	const unsigned int pixel_line_clock = frame_cycles_;// + 128 - first_graphics_cycle + 80;
//	const unsigned int line_before_cycle = graphics_line(pixel_line_clock);
//	const unsigned int line_after_cycle = graphics_line(pixel_line_clock + cycles);

	// implicit assumption here: the number of 2Mhz cycles this bus operation will take
	// is never longer than a line. On the Electron, it's a safe one.
//	if(line_before_cycle != line_after_cycle)
//	{
//		switch(line_before_cycle)
//		{
//			case real_time_clock_interrupt_line:	signal_interrupt(Interrupt::RealTimeClock);	break;
//			case real_time_clock_interrupt_line+1:	clear_interrupt(Interrupt::RealTimeClock);	break;
//			case display_end_interrupt_line:		signal_interrupt(Interrupt::DisplayEnd);	break;
//			case display_end_interrupt_line+1:		clear_interrupt(Interrupt::DisplayEnd);		break;
//		}
//	}

//	if(
//		(pixel_line_clock < real_time_clock_interrupt_1 && pixel_line_clock + cycles >= real_time_clock_interrupt_1) ||
//		(pixel_line_clock < real_time_clock_interrupt_2 && pixel_line_clock + cycles >= real_time_clock_interrupt_2))
//	{
//		signal_interrupt(Interrupt::RealTimeClock);
//	}

//	frame_cycles_ += cycles;

	// deal with frame wraparound by updating the two dependent subsystems
	// as though the exact end of frame had been hit, then reset those
	// and allow the frame cycle counter to assume its real value
//	if(frame_cycles_ >= cycles_per_frame)
//	{
//		unsigned int nextFrameCycles = frame_cycles_ - cycles_per_frame;
//		frame_cycles_ = cycles_per_frame;
//		update_display();
//		update_audio();
//		display_output_position_ = 0;
//		audio_output_position_ = 0;
//		frame_cycles_ = nextFrameCycles;
//	}

//	if(!(frame_cycles_&16383))
//		update_audio();
//	return 0;
//}

VideoOutput::Interrupt VideoOutput::get_next_interrupt()
{
	VideoOutput::Interrupt interrupt;

	if(output_position_ < real_time_clock_interrupt_1)
	{
		interrupt.cycles = real_time_clock_interrupt_1 - output_position_;
		interrupt.interrupt = RealTimeClock;
		return interrupt;
	}

	if(output_position_ < display_end_interrupt_1)
	{
		interrupt.cycles = display_end_interrupt_1 - output_position_;
		interrupt.interrupt = DisplayEnd;
		return interrupt;
	}

	if(output_position_ < real_time_clock_interrupt_2)
	{
		interrupt.cycles = real_time_clock_interrupt_2 - output_position_;
		interrupt.interrupt = RealTimeClock;
		return interrupt;
	}

	if(output_position_ < display_end_interrupt_2)
	{
		interrupt.cycles = display_end_interrupt_2 - output_position_;
		interrupt.interrupt = DisplayEnd;
		return interrupt;
	}

	interrupt.cycles = real_time_clock_interrupt_1 + cycles_per_frame - output_position_;
	interrupt.interrupt = RealTimeClock;
	return interrupt;
}

#pragma mark - RAM timing

unsigned int VideoOutput::get_cycles_until_next_ram_availability(int from_time)
{
	unsigned int result = 0;
	int position = output_position_ + from_time;

	result += 1 + (position&1);
//	if(screen_mode_ < 4)
//	{
//		const int current_line = graphics_line(position + (position&1));
//		const int current_column = graphics_column(position + (position&1));
//		if(current_line < 256 && current_column < 80 && !is_blank_line_)
//			result += (unsigned int)(80 - current_column);
//	}
	return result;
}
