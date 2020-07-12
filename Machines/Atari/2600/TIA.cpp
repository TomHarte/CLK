//
//  TIA.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 28/01/2017.
//  Copyright 2017 Thomas Harte. All rights reserved.
//

#include "TIA.hpp"

#include <cassert>
#include <cstring>

using namespace Atari2600;
namespace {
	constexpr int cycles_per_line = 228;
	constexpr int first_pixel_cycle = 68;

	constexpr int sync_flag	= 0x1;
	constexpr int blank_flag = 0x2;

	uint8_t reverse_table[256];
}

TIA::TIA():
 	crt_(cycles_per_line * 2 - 1, 1, Outputs::Display::Type::NTSC60, Outputs::Display::InputDataType::Luminance8Phase8) {

	set_output_mode(OutputMode::NTSC);

	for(int c = 0; c < 256; c++) {
		reverse_table[c] = uint8_t(
			((c & 0x01) << 7) | ((c & 0x02) << 5) | ((c & 0x04) << 3) | ((c & 0x08) << 1) |
			((c & 0x10) >> 1) | ((c & 0x20) >> 3) | ((c & 0x40) >> 5) | ((c & 0x80) >> 7)
		);
	}

	for(int c = 0; c < 64; c++) {
		bool has_playfield = c & int(CollisionType::Playfield);
		bool has_ball = c & int(CollisionType::Ball);
		bool has_player0 = c & int(CollisionType::Player0);
		bool has_player1 = c & int(CollisionType::Player1);
		bool has_missile0 = c & int(CollisionType::Missile0);
		bool has_missile1 = c & int(CollisionType::Missile1);

		uint8_t collision_registers[8];
		collision_registers[0] = ((has_missile0 && has_player1) ? 0x80 : 0x00)		|	((has_missile0 && has_player0) ? 0x40 : 0x00);
		collision_registers[1] = ((has_missile1 && has_player0) ? 0x80 : 0x00)		|	((has_missile1 && has_player1) ? 0x40 : 0x00);
		collision_registers[2] = ((has_playfield && has_player0) ? 0x80 : 0x00)		|	((has_ball && has_player0) ? 0x40 : 0x00);
		collision_registers[3] = ((has_playfield && has_player1) ? 0x80 : 0x00)		|	((has_ball && has_player1) ? 0x40 : 0x00);
		collision_registers[4] = ((has_playfield && has_missile0) ? 0x80 : 0x00)	|	((has_ball && has_missile0) ? 0x40 : 0x00);
		collision_registers[5] = ((has_playfield && has_missile1) ? 0x80 : 0x00)	|	((has_ball && has_missile1) ? 0x40 : 0x00);
		collision_registers[6] = ((has_playfield && has_ball) ? 0x80 : 0x00);
		collision_registers[7] = ((has_player0 && has_player1) ? 0x80 : 0x00)		|	((has_missile0 && has_missile1) ? 0x40 : 0x00);
		collision_flags_by_buffer_vaules_[c] =
			(collision_registers[0] >> 6) |
			(collision_registers[1] >> 4) |
			(collision_registers[2] >> 2) |
			(collision_registers[3] >> 0) |
			(collision_registers[4] << 2) |
			(collision_registers[5] << 4) |
			(collision_registers[6] << 6) |
			(collision_registers[7] << 8);

		// all priority modes show the background if nothing else is present
		colour_mask_by_mode_collision_flags_[int(ColourMode::Standard)][c] =
		colour_mask_by_mode_collision_flags_[int(ColourMode::ScoreLeft)][c] =
		colour_mask_by_mode_collision_flags_[int(ColourMode::ScoreRight)][c] =
		colour_mask_by_mode_collision_flags_[int(ColourMode::OnTop)][c] = uint8_t(ColourIndex::Background);

		// test 1 for standard priority: if there is a playfield or ball pixel, plot that colour
		if(has_playfield || has_ball) {
			colour_mask_by_mode_collision_flags_[int(ColourMode::Standard)][c] = uint8_t(ColourIndex::PlayfieldBall);
		}

		// test 1 for score mode: if there is a ball pixel, plot that colour
		if(has_ball) {
			colour_mask_by_mode_collision_flags_[int(ColourMode::ScoreLeft)][c] =
			colour_mask_by_mode_collision_flags_[int(ColourMode::ScoreRight)][c] = uint8_t(ColourIndex::PlayfieldBall);
		}

		// test 1 for on-top mode, test 2 for everbody else: if there is a player 1 or missile 1 pixel, plot that colour
		if(has_player1 || has_missile1) {
			colour_mask_by_mode_collision_flags_[int(ColourMode::Standard)][c] =
			colour_mask_by_mode_collision_flags_[int(ColourMode::ScoreLeft)][c] =
			colour_mask_by_mode_collision_flags_[int(ColourMode::ScoreRight)][c] =
			colour_mask_by_mode_collision_flags_[int(ColourMode::OnTop)][c] = uint8_t(ColourIndex::PlayerMissile1);
		}

		// in the right-hand side of score mode, the playfield has the same priority as player 1
		if(has_playfield) {
			colour_mask_by_mode_collision_flags_[int(ColourMode::ScoreRight)][c] = uint8_t(ColourIndex::PlayerMissile1);
		}

		// next test for everybody: if there is a player 0 or missile 0 pixel, plot that colour instead
		if(has_player0 || has_missile0) {
			colour_mask_by_mode_collision_flags_[int(ColourMode::Standard)][c] =
			colour_mask_by_mode_collision_flags_[int(ColourMode::ScoreLeft)][c] =
			colour_mask_by_mode_collision_flags_[int(ColourMode::ScoreRight)][c] =
			colour_mask_by_mode_collision_flags_[int(ColourMode::OnTop)][c] = uint8_t(ColourIndex::PlayerMissile0);
		}

		// if this is the left-hand side of score mode, the playfield has the same priority as player 0
		if(has_playfield) {
			colour_mask_by_mode_collision_flags_[int(ColourMode::ScoreLeft)][c] = uint8_t(ColourIndex::PlayerMissile0);
		}

		// a final test for 'on top' priority mode: if the playfield or ball are visible, prefer that colour to all others
		if(has_playfield || has_ball) {
			colour_mask_by_mode_collision_flags_[int(ColourMode::OnTop)][c] = uint8_t(ColourIndex::PlayfieldBall);
		}
	}
}

void TIA::set_output_mode(Atari2600::TIA::OutputMode output_mode) {
	Outputs::Display::Type display_type;
	tv_standard_ = output_mode;

	if(output_mode == OutputMode::NTSC) {
		display_type = Outputs::Display::Type::NTSC60;
	} else {
		display_type = Outputs::Display::Type::PAL50;
	}
	crt_.set_display_type(Outputs::Display::DisplayType::CompositeColour);

	// line number of cycles in a line of video is one less than twice the number of clock cycles per line; the Atari
	// outputs 228 colour cycles of material per line when an NTSC line 227.5. Since all clock numbers will be doubled
	// later, cycles_per_line * 2 - 1 is therefore the real length of an NTSC line, even though we're going to supply
	// cycles_per_line * 2 cycles of information from one sync edge to the next
	crt_.set_new_display_type(cycles_per_line * 2 - 1, display_type);

	// Update the luminance/phase mappings of the current palette.
	for(size_t c = 0; c < colour_palette_.size(); ++c) {
		set_colour_palette_entry(c, colour_palette_[c].original);
	}
}

void TIA::set_crt_delegate(Outputs::CRT::Delegate *delegate) {
	crt_.set_delegate(delegate);
}

void TIA::set_scan_target(Outputs::Display::ScanTarget *scan_target) {
	crt_.set_scan_target(scan_target);
}

Outputs::Display::ScanStatus TIA::get_scaled_scan_status() const {
	return crt_.get_scaled_scan_status() / 2.0f;
}

void TIA::run_for(const Cycles cycles) {
	int number_of_cycles = int(cycles.as_integral());

	// if part way through a line, definitely perform a partial, at most up to the end of the line
	if(horizontal_counter_) {
		int output_cycles = std::min(number_of_cycles, cycles_per_line - horizontal_counter_);
		output_for_cycles(output_cycles);
		number_of_cycles -= output_cycles;
	}

	// output full lines for as long as possible
	while(number_of_cycles >= cycles_per_line) {
		output_line();
		number_of_cycles -= cycles_per_line;
	}

	// partly start a new line if necessary
	if(number_of_cycles) {
		output_for_cycles(number_of_cycles);
	}
}

void TIA::set_sync(bool sync) {
	output_mode_ = (output_mode_ & ~sync_flag) | (sync ? sync_flag : 0);
}

void TIA::set_blank(bool blank) {
	output_mode_ = (output_mode_ & ~blank_flag) | (blank ? blank_flag : 0);
}

void TIA::reset_horizontal_counter() {
}

int TIA::get_cycles_until_horizontal_blank(const Cycles from_offset) {
	return (cycles_per_line - (horizontal_counter_ + from_offset.as_integral()) % cycles_per_line) % cycles_per_line;
}

void TIA::set_background_colour(uint8_t colour) {
	set_colour_palette_entry(size_t(ColourIndex::Background), colour);
}

void TIA::set_colour_palette_entry(size_t index, uint8_t colour) {
	const uint8_t luminance = ((colour & 14) * 255) / 14;

	uint8_t phase = colour >> 4;

	if(tv_standard_ == OutputMode::NTSC) {
		if(!phase) phase = 255;
		else {
			phase = -(phase * 127) / 13;
			phase -= 102;
			phase &= 127;
		}
	} else {
		if(phase < 2 || phase > 13) {
			phase = 255;
		} else {
			const auto direction = phase & 1;

			phase >>= 1;
			if(direction) phase ^= 0xf;
			phase = (phase + 6 + direction) & 0xf;

			phase = (phase * 127) / 12;
			phase &= 127;
		}
	}

	colour_palette_[index].original = colour;
	uint8_t *target = reinterpret_cast<uint8_t *>(&colour_palette_[index].luminance_phase);
	target[0] = luminance;
	target[1] = phase;
}

void TIA::set_playfield(uint16_t offset, uint8_t value) {
	assert(offset >= 0 && offset < 3);
	switch(offset) {
		case 0:
			background_[1] = (background_[1] & 0x0ffff) | (uint32_t(reverse_table[value & 0xf0]) << 16);
			background_[0] = (background_[0] & 0xffff0) | uint32_t(value >> 4);
		break;
		case 1:
			background_[1] = (background_[1] & 0xf00ff) | (uint32_t(value) << 8);
			background_[0] = (background_[0] & 0xff00f) | (uint32_t(reverse_table[value]) << 4);
		break;
		case 2:
			background_[1] = (background_[1] & 0xfff00) | reverse_table[value];
			background_[0] = (background_[0] & 0x00fff) | (uint32_t(value) << 12);
		break;
	}
}

void TIA::set_playfield_control_and_ball_size(uint8_t value) {
	background_half_mask_ = value & 1;
	switch(value & 6) {
		case 0:
			playfield_priority_ = PlayfieldPriority::Standard;
		break;
		case 2:
			playfield_priority_ = PlayfieldPriority::Score;
		break;
		case 4:
		case 6:
			playfield_priority_ = PlayfieldPriority::OnTop;
		break;
	}

	ball_.size = 1 << ((value >> 4)&3);
}

void TIA::set_playfield_ball_colour(uint8_t colour) {
	set_colour_palette_entry(size_t(ColourIndex::PlayfieldBall), colour);
}

void TIA::set_player_number_and_size(int player, uint8_t value) {
	assert(player >= 0 && player < 2);
	int size = 0;
	switch(value & 7) {
		case 0: case 1: case 2: case 3: case 4:
			player_[player].copy_flags = value & 7;
		break;
		case 5:
			size = 1;
			player_[player].copy_flags = 0;
		break;
		case 6:
			player_[player].copy_flags = 6;
		break;
		case 7:
			size = 2;
			player_[player].copy_flags = 0;
		break;
	}

	missile_[player].size = 1 << ((value >> 4)&3);
	missile_[player].copy_flags = player_[player].copy_flags;
	player_[player].adder = 4 >> size;
}

void TIA::set_player_graphic(int player, uint8_t value) {
	assert(player >= 0 && player < 2);
	player_[player].graphic[1] = value;
	player_[player^1].graphic[0] = player_[player^1].graphic[1];
	if(player) ball_.enabled[0] = ball_.enabled[1];
}

void TIA::set_player_reflected(int player, bool reflected) {
	assert(player >= 0 && player < 2);
	player_[player].reverse_mask = reflected ? 7 : 0;
}

void TIA::set_player_delay(int player, bool delay) {
	assert(player >= 0 && player < 2);
	player_[player].graphic_index = delay ? 0 : 1;
}

void TIA::set_player_position(int player) {
	assert(player >= 0 && player < 2);
	// players have an extra clock of delay before output and don't display upon reset;
	// both aims are achieved by setting to -1 because: (i) it causes the clock to be
	// one behind its real hardware value, creating the extra delay; and (ii) the player
	// code is written to start a draw upon wraparound from 159 to 0, so -1 is the
	// correct option rather than 159.
	player_[player].position = -1;
}

void TIA::set_player_motion(int player, uint8_t motion) {
	assert(player >= 0 && player < 2);
	player_[player].motion = (motion >> 4)&0xf;
}

void TIA::set_player_missile_colour(int player, uint8_t colour) {
	assert(player >= 0 && player < 2);
	set_colour_palette_entry(size_t(ColourIndex::PlayerMissile0) + size_t(player), colour);
}

void TIA::set_missile_enable(int missile, bool enabled) {
	assert(missile >= 0 && missile < 2);
	missile_[missile].enabled = enabled;
}

void TIA::set_missile_position(int missile) {
	assert(missile >= 0 && missile < 2);
	missile_[missile].position = 0;
}

void TIA::set_missile_position_to_player(int missile, bool lock) {
	assert(missile >= 0 && missile < 2);
	missile_[missile].locked_to_player = lock;
	player_[missile].latched_pixel4_time = -1;
}

void TIA::set_missile_motion(int missile, uint8_t motion) {
	assert(missile >= 0 && missile < 2);
	missile_[missile].motion = (motion >> 4)&0xf;
}

void TIA::set_ball_enable(bool enabled) {
	ball_.enabled[1] = enabled;
}

void TIA::set_ball_delay(bool delay) {
	ball_.enabled_index = delay ? 0 : 1;
}

void TIA::set_ball_position() {
	ball_.position = 0;

	// setting the ball position also triggers a draw
	ball_.reset_pixels(0);
}

void TIA::set_ball_motion(uint8_t motion) {
	ball_.motion = (motion >> 4) & 0xf;
}

void TIA::move() {
	horizontal_blank_extend_ = true;
	player_[0].is_moving = player_[1].is_moving = missile_[0].is_moving = missile_[1].is_moving = ball_.is_moving = true;
	player_[0].motion_step = player_[1].motion_step = missile_[0].motion_step = missile_[1].motion_step = ball_.motion_step = 15;
	player_[0].motion_time = player_[1].motion_time = missile_[0].motion_time = missile_[1].motion_time = ball_.motion_time = (horizontal_counter_ + 3) & ~3;
}

void TIA::clear_motion() {
	player_[0].motion = player_[1].motion = missile_[0].motion = missile_[1].motion = ball_.motion = 0;
}

uint8_t TIA::get_collision_flags(int offset) {
	return uint8_t((collision_flags_ >> (offset << 1)) << 6) & 0xc0;
}

void TIA::clear_collision_flags() {
	collision_flags_ = 0;
}

void TIA::output_for_cycles(int number_of_cycles) {
	/*
		Line timing is oriented around 0 being the start of the right-hand side vertical blank;
		a wsync synchronises the CPU to horizontal_counter_ = 0. All timing below is in terms of the
		NTSC colour clock.

		Therefore, each line is composed of:

			16 cycles:	blank					; -> 16
			16 cycles:	sync					; -> 32
			16 cycles:	colour burst			; -> 48
			20 cycles:	blank					; -> 68
			8 cycles:	blank or pixels, depending on whether the blank extend bit is set
			152 cycles:	pixels
	*/
	int output_cursor = horizontal_counter_;
	horizontal_counter_ += number_of_cycles;
	bool is_reset = output_cursor < 224 && horizontal_counter_ >= 224;

	if(!output_cursor) {
		std::memset(collision_buffer_, 0, sizeof(collision_buffer_));

		ball_.motion_time %= 228;
		player_[0].motion_time %= 228;
		player_[1].motion_time %= 228;
		missile_[0].motion_time %= 228;
		missile_[1].motion_time %= 228;
	}

	// accumulate an OR'd version of the output into the collision buffer
	int latent_start = output_cursor + 4;
	int latent_end = horizontal_counter_ + 4;
	draw_playfield(latent_start, latent_end);
	draw_object<Player>(player_[0], uint8_t(CollisionType::Player0), output_cursor, horizontal_counter_);
	draw_object<Player>(player_[1], uint8_t(CollisionType::Player1), output_cursor, horizontal_counter_);
	draw_missile(missile_[0], player_[0], uint8_t(CollisionType::Missile0), output_cursor, horizontal_counter_);
	draw_missile(missile_[1], player_[1], uint8_t(CollisionType::Missile1), output_cursor, horizontal_counter_);
	draw_object<Ball>(ball_, uint8_t(CollisionType::Ball), output_cursor, horizontal_counter_);

	// convert to television signals

#define Period(function, target)	\
	if(output_cursor < target) { \
		if(horizontal_counter_ <= target) { \
			crt_.function((horizontal_counter_ - output_cursor) * 2); \
			horizontal_counter_ %= cycles_per_line; \
			return; \
		} else { \
			crt_.function((target - output_cursor) * 2); \
			output_cursor = target; \
		} \
	}

	switch(output_mode_) {
		default:
			Period(output_blank, 16)
			Period(output_sync, 32)
			Period(output_default_colour_burst, 48)
			Period(output_blank, 68)
		break;
		case sync_flag:
		case sync_flag | blank_flag:
			Period(output_sync, 16)
			Period(output_blank, 32)
			Period(output_default_colour_burst, 48)
			Period(output_sync, 228)
		break;
	}

#undef Period

	if(output_mode_ & blank_flag) {
		if(pixel_target_) {
			output_pixels(pixels_start_location_, output_cursor);
			const int data_length = int(output_cursor - pixels_start_location_);
			crt_.output_data(data_length * 2, size_t(data_length));
			pixel_target_ = nullptr;
			pixels_start_location_ = 0;
		}
		int duration = std::min(228, horizontal_counter_) - output_cursor;
		crt_.output_blank(duration * 2);
	} else {
		if(!pixels_start_location_) {
			pixels_start_location_ = output_cursor;
			pixel_target_ = reinterpret_cast<uint16_t *>(crt_.begin_data(160));
		}

		// convert that into pixels
		if(pixel_target_) output_pixels(output_cursor, horizontal_counter_);

		// accumulate collision flags
		while(output_cursor < horizontal_counter_) {
			collision_flags_ |= collision_flags_by_buffer_vaules_[collision_buffer_[output_cursor - first_pixel_cycle]];
			output_cursor++;
		}

		if(horizontal_counter_ == cycles_per_line) {
			const int data_length = int(output_cursor - pixels_start_location_);
			crt_.output_data(data_length * 2, size_t(data_length));
			pixel_target_ = nullptr;
			pixels_start_location_ = 0;
		}
	}

	if(is_reset) horizontal_blank_extend_ = false;

	horizontal_counter_ %= cycles_per_line;
}

void TIA::output_pixels(int start, int end) {
	start = std::max(start, pixels_start_location_);
	int target_position = start - pixels_start_location_;

	if(start < first_pixel_cycle+8 && horizontal_blank_extend_) {
		while(start < end && start < first_pixel_cycle+8) {
			pixel_target_[target_position] = 0xff00;	// TODO: this assumes little endianness.
			start++;
			target_position++;
		}
	}

	if(playfield_priority_ == PlayfieldPriority::Score) {
		while(start < end && start < first_pixel_cycle + 80) {
			uint8_t buffer_value = collision_buffer_[start - first_pixel_cycle];
			pixel_target_[target_position] = colour_palette_[colour_mask_by_mode_collision_flags_[int(ColourMode::ScoreLeft)][buffer_value]].luminance_phase;
			start++;
			target_position++;
		}
		while(start < end) {
			uint8_t buffer_value = collision_buffer_[start - first_pixel_cycle];
			pixel_target_[target_position] = colour_palette_[colour_mask_by_mode_collision_flags_[int(ColourMode::ScoreRight)][buffer_value]].luminance_phase;
			start++;
			target_position++;
		}
	} else {
		int table_index = int((playfield_priority_ == PlayfieldPriority::Standard) ? ColourMode::Standard : ColourMode::OnTop);
		while(start < end) {
			uint8_t buffer_value = collision_buffer_[start - first_pixel_cycle];
			pixel_target_[target_position] = colour_palette_[colour_mask_by_mode_collision_flags_[table_index][buffer_value]].luminance_phase;
			start++;
			target_position++;
		}
	}
}

void TIA::output_line() {
	switch(output_mode_) {
		default:
			// TODO: optimise special case
			output_for_cycles(cycles_per_line);
		break;
		case sync_flag:
		case sync_flag | blank_flag:
			crt_.output_sync(32);
			crt_.output_blank(32);
			crt_.output_sync(392);
			horizontal_blank_extend_ = false;
		break;
		case blank_flag:
			crt_.output_blank(32);
			crt_.output_sync(32);
			crt_.output_default_colour_burst(32);
			crt_.output_blank(360);
			horizontal_blank_extend_ = false;
		break;
	}
}

// MARK: - Playfield output

void TIA::draw_playfield(int start, int end) {
	// don't do anything if this window ends too early
	if(end < first_pixel_cycle) return;

	// clip to drawable bounds
	start = std::max(start, first_pixel_cycle);
	end = std::min(end, 228);

	// proceed along four-pixel boundaries, plotting four pixels at a time
	int aligned_position = (start + 3)&~3;
	while(aligned_position < end) {
		int offset = (aligned_position - first_pixel_cycle) >> 2;
		uint32_t value = ((background_[(offset/20)&background_half_mask_] >> (offset%20))&1) * 0x01010101;
		*reinterpret_cast<uint32_t *>(&collision_buffer_[aligned_position - first_pixel_cycle]) |= value;
		aligned_position += 4;
	}
}

// MARK: - Motion

template<class T> void TIA::perform_motion_step(T &object) {
	if((object.motion_step ^ (object.motion ^ 8)) == 0xf) {
		object.is_moving = false;
	} else {
		if(object.position == 159) object.reset_pixels(0);
		else if(object.position == 15 && object.copy_flags&1) object.reset_pixels(1);
		else if(object.position == 31 && object.copy_flags&2) object.reset_pixels(2);
		else if(object.position == 63 && object.copy_flags&4) object.reset_pixels(3);
		else object.skip_pixels(1, object.motion_time);
		object.position = (object.position + 1) % 160;
		object.motion_step --;
		object.motion_time += 4;
	}
}

template<class T> void TIA::perform_border_motion(T &object, int, int end) {
	while(object.is_moving && object.motion_time < end)
		perform_motion_step<T>(object);
}

template<class T> void TIA::draw_object(T &object, const uint8_t collision_identity, int start, int end) {
	int first_pixel = first_pixel_cycle - 4 + (horizontal_blank_extend_ ? 8 : 0);

	object.dequeue_pixels(collision_buffer_, collision_identity, end - first_pixel_cycle);

	// movement works across the entire screen, so do work that falls outside of the pixel area
	if(start < first_pixel) {
		perform_border_motion<T>(object, start, std::min(end, first_pixel));
	}

	// don't continue to do any drawing if this window ends too early
	if(end < first_pixel) return;
	if(start < first_pixel) start = first_pixel;
	if(start >= end) return;

	// perform the visible part of the line, if any
	if(start < 224) {
		draw_object_visible<T>(object, collision_identity, start - first_pixel_cycle + 4, std::min(end - first_pixel_cycle + 4, 160), end - first_pixel_cycle);
	}

	// move further if required
	if(object.is_moving && end >= 224 && object.motion_time < end) {
		perform_motion_step<T>(object);
	}
}

template<class T> void TIA::draw_object_visible(T &object, const uint8_t collision_identity, int start, int end, int time_now) {
	// perform a miniature event loop on (i) triggering draws; (ii) drawing; and (iii) motion
	int next_motion_time = object.motion_time - first_pixel_cycle + 4;
	while(start < end) {
		int next_event_time = end;

		// is the next event a movement tick?
		if(object.is_moving && next_motion_time < next_event_time) {
			next_event_time = next_motion_time;
		}

		// is the next event a graphics trigger?
		int next_copy = 160;
		int next_copy_id = 0;
		if(object.copy_flags) {
			if(object.position < 16 && object.copy_flags&1) {
				next_copy = 16;
				next_copy_id = 1;
			} else if(object.position < 32 && object.copy_flags&2) {
				next_copy = 32;
				next_copy_id = 2;
			} else if(object.position < 64 && object.copy_flags&4) {
				next_copy = 64;
				next_copy_id = 3;
			}
		}

		int next_copy_time = start + next_copy - object.position;
		if(next_copy_time < next_event_time) next_event_time = next_copy_time;

		// the decision is to progress by length
		const int length = next_event_time - start;

		// enqueue a future intention to draw pixels if spitting them out now would violate accuracy;
		// otherwise draw them now
		if(object.enqueues && next_event_time > time_now) {
			if(start < time_now) {
				object.output_pixels(&collision_buffer_[start], time_now - start, collision_identity, start + first_pixel_cycle - 4);
				object.enqueue_pixels(time_now, next_event_time, time_now + first_pixel_cycle - 4);
			} else {
				object.enqueue_pixels(start, next_event_time, start + first_pixel_cycle - 4);
			}
		} else {
			object.output_pixels(&collision_buffer_[start], length, collision_identity, start + first_pixel_cycle - 4);
		}

		// the next interesting event is after next_event_time cycles, so progress
		object.position = (object.position + length) % 160;
		start = next_event_time;

		// if the event is a motion tick, apply; if it's a draw trigger, trigger a draw
		if(object.is_moving && start == next_motion_time) {
			perform_motion_step(object);
			next_motion_time += 4;
		} else if(start == next_copy_time) {
			object.reset_pixels(next_copy_id);
		}
	}
}

// MARK: - Missile drawing

void TIA::draw_missile(Missile &missile, Player &player, const uint8_t collision_identity, int start, int end) {
	if(!missile.locked_to_player || player.latched_pixel4_time < 0) {
		draw_object<Missile>(missile, collision_identity, start, end);
	} else {
		draw_object<Missile>(missile, collision_identity, start, player.latched_pixel4_time);
		missile.position = 0;
		draw_object<Missile>(missile, collision_identity, player.latched_pixel4_time, end);
		player.latched_pixel4_time = -1;
	}
}
