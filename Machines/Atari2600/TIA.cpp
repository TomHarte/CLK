//
//  TIA.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 28/01/2017.
//  Copyright © 2017 Thomas Harte. All rights reserved.
//

#include "TIA.hpp"

using namespace Atari2600;
namespace {
	const int cycles_per_line = 228;
	const int first_pixel_cycle = 68;

	const int sync_flag	= 0x1;
	const int blank_flag = 0x2;

	uint8_t reverse_table[256];
}

TIA::TIA() :
	horizontal_counter_(0),
	pixels_start_location_(0),
	output_mode_(0),
	pixel_target_(nullptr),
	background_{0, 0},
	background_half_mask_(0)
{
	crt_.reset(new Outputs::CRT::CRT(cycles_per_line * 2 + 1, 1, Outputs::CRT::DisplayType::NTSC60, 1));
	crt_->set_output_device(Outputs::CRT::Television);
	set_output_mode(OutputMode::NTSC);

	for(int c = 0; c < 256; c++)
	{
		reverse_table[c] = (uint8_t)(
			((c & 0x01) << 7) | ((c & 0x02) << 5) | ((c & 0x04) << 3) | ((c & 0x08) << 1) |
			((c & 0x10) >> 1) | ((c & 0x20) >> 3) | ((c & 0x40) >> 5) | ((c & 0x80) >> 7)
		);
	}

	for(int c = 0; c < 64; c++)
	{
		bool has_playfield = c & (int)(CollisionType::Playfield);
		bool has_ball = c & (int)(CollisionType::Ball);
		bool has_player0 = c & (int)(CollisionType::Player0);
		bool has_player1 = c & (int)(CollisionType::Player1);
		bool has_missile0 = c & (int)(CollisionType::Missile0);
		bool has_missile1 = c & (int)(CollisionType::Missile1);

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
		colour_mask_by_mode_collision_flags_[(int)ColourMode::Standard][c] =
		colour_mask_by_mode_collision_flags_[(int)ColourMode::ScoreLeft][c] =
		colour_mask_by_mode_collision_flags_[(int)ColourMode::ScoreRight][c] =
		colour_mask_by_mode_collision_flags_[(int)ColourMode::OnTop][c] = (uint8_t)ColourIndex::Background;

		// test 1 for standard priority: if there is a playfield or ball pixel, plot that colour
		if(has_playfield || has_ball)
		{
			colour_mask_by_mode_collision_flags_[(int)ColourMode::Standard][c] = (uint8_t)ColourIndex::PlayfieldBall;
		}

		// test 1 for score mode: if there is a ball pixel, plot that colour
		if(has_ball)
		{
			colour_mask_by_mode_collision_flags_[(int)ColourMode::ScoreLeft][c] =
			colour_mask_by_mode_collision_flags_[(int)ColourMode::ScoreRight][c] = (uint8_t)ColourIndex::PlayfieldBall;
		}

		// test 1 for on-top mode, test 2 for everbody else: if there is a player 1 or missile 1 pixel, plot that colour
		if(has_player1 || has_missile1)
		{
			colour_mask_by_mode_collision_flags_[(int)ColourMode::Standard][c] =
			colour_mask_by_mode_collision_flags_[(int)ColourMode::ScoreLeft][c] =
			colour_mask_by_mode_collision_flags_[(int)ColourMode::ScoreRight][c] =
			colour_mask_by_mode_collision_flags_[(int)ColourMode::OnTop][c] = (uint8_t)ColourIndex::PlayerMissile1;
		}

		// in the right-hand side of score mode, the playfield has the same priority as player 1
		if(has_playfield)
		{
			colour_mask_by_mode_collision_flags_[(int)ColourMode::ScoreRight][c] = (uint8_t)ColourIndex::PlayerMissile1;
		}

		// next test for everybody: if there is a player 0 or missile 0 pixel, plot that colour instead
		if(has_player0 || has_missile0)
		{
			colour_mask_by_mode_collision_flags_[(int)ColourMode::Standard][c] =
			colour_mask_by_mode_collision_flags_[(int)ColourMode::ScoreLeft][c] =
			colour_mask_by_mode_collision_flags_[(int)ColourMode::ScoreRight][c] =
			colour_mask_by_mode_collision_flags_[(int)ColourMode::OnTop][c] = (uint8_t)ColourIndex::PlayerMissile0;
		}

		// if this is the left-hand side of score mode, the playfield has the same priority as player 0
		if(has_playfield)
		{
			colour_mask_by_mode_collision_flags_[(int)ColourMode::ScoreLeft][c] = (uint8_t)ColourIndex::PlayerMissile0;
		}

		// a final test for 'on top' priority mode: if the playfield or ball are visible, prefer that colour to all others
		if(has_playfield || has_ball)
		{
			colour_mask_by_mode_collision_flags_[(int)ColourMode::OnTop][c] = (uint8_t)ColourIndex::PlayfieldBall;
		}
	}
}

void TIA::set_output_mode(Atari2600::TIA::OutputMode output_mode)
{
	// this is the NTSC phase offset function; see below for PAL
	crt_->set_composite_sampling_function(
		"float composite_sample(usampler2D texID, vec2 coordinate, vec2 iCoordinate, float phase, float amplitude)"
		"{"
			"uint c = texture(texID, coordinate).r;"
			"uint y = c & 14u;"
			"uint iPhase = (c >> 4);"

			"float phaseOffset = 6.283185308 * float(iPhase) / 13.0  + 5.074880441076923;"
			"return mix(float(y) / 14.0, step(1, iPhase) * cos(phase + phaseOffset), amplitude);"
		"}");
/*	speaker_->set_input_rate((float)(get_clock_rate() / 38.0));*/
}

TIA::~TIA()
{
}

/*void Machine::switch_region()
{
	// the PAL function
	crt_->set_composite_sampling_function(
		"float composite_sample(usampler2D texID, vec2 coordinate, vec2 iCoordinate, float phase, float amplitude)"
		"{"
			"uint c = texture(texID, coordinate).r;"
			"uint y = c & 14u;"
			"uint iPhase = (c >> 4);"

			"uint direction = iPhase & 1u;"
			"float phaseOffset = float(7u - direction) + (float(direction) - 0.5) * 2.0 * float(iPhase >> 1);"
			"phaseOffset *= 6.283185308 / 12.0;"
			"return mix(float(y) / 14.0, step(4, (iPhase + 2u) & 15u) * cos(phase + phaseOffset), amplitude);"
		"}");

	crt_->set_new_timing(228, 312, Outputs::CRT::ColourSpace::YUV, 228, 1, true);

	is_pal_region_ = true;
	speaker_->set_input_rate((float)(get_clock_rate() / 38.0));
	set_clock_rate(PAL_clock_rate);
}*/

						// justification for +5: "we need to wait at least 71 [clocks] before the HMOVE operation is complete";
						// which will take 16*4 + 2 = 66 cycles from the first compare, implying the first compare must be
						// in five cycles from now


void TIA::run_for_cycles(int number_of_cycles)
{
	// if part way through a line, definitely perform a partial, at most up to the end of the line
	if(horizontal_counter_)
	{
		int cycles = std::min(number_of_cycles, cycles_per_line - horizontal_counter_);
		output_for_cycles(cycles);
		number_of_cycles -= cycles;
	}

	// output full lines for as long as possible
	while(number_of_cycles >= cycles_per_line)
	{
		output_line();
		number_of_cycles -= cycles_per_line;
	}

	// partly start a new line if necessary
	if(number_of_cycles)
	{
		output_for_cycles(number_of_cycles);
	}
}

void TIA::set_sync(bool sync)
{
	output_mode_ = (output_mode_ & ~sync_flag) | (sync ? sync_flag : 0);
}

void TIA::set_blank(bool blank)
{
	output_mode_ = (output_mode_ & ~blank_flag) | (blank ? blank_flag : 0);
}

void TIA::reset_horizontal_counter()
{
}

int TIA::get_cycles_until_horizontal_blank(unsigned int from_offset)
{
	return (cycles_per_line - (horizontal_counter_ + (int)from_offset) % cycles_per_line) % cycles_per_line;
}

void TIA::set_background_colour(uint8_t colour)
{
	colour_palette_[(int)ColourIndex::Background] = colour;
}

void TIA::set_playfield(uint16_t offset, uint8_t value)
{
	switch(offset)
	{
		case 0:
			background_[1] = (background_[1] & 0x0ffff) | ((uint32_t)reverse_table[value & 0xf0] << 16);
			background_[0] = (background_[0] & 0xffff0) | (uint32_t)(value >> 4);
		break;
		case 1:
			background_[1] = (background_[1] & 0xf00ff) | ((uint32_t)value << 8);
			background_[0] = (background_[0] & 0xff00f) | ((uint32_t)reverse_table[value] << 4);
		break;
		case 2:
			background_[1] = (background_[1] & 0xfff00) | reverse_table[value];
			background_[0] = (background_[0] & 0x00fff) | ((uint32_t)value << 12);
		break;
	}
}

void TIA::set_playfield_control_and_ball_size(uint8_t value)
{
	background_half_mask_ = value & 1;
	switch(value & 6)
	{
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
}

void TIA::set_playfield_ball_colour(uint8_t colour)
{
	colour_palette_[(int)ColourIndex::PlayfieldBall] = colour;
}

void TIA::set_player_number_and_size(int player, uint8_t value)
{
	switch(value & 7)
	{
		case 0: case 1: case 2: case 3: case 4:
			player_[player].size = 0;
			player_[player].copy_flags = value & 7;
		break;
		case 5:
			player_[player].size = 1;
			player_[player].copy_flags = 0;
		break;
		case 6:
			player_[player].size = 0;
			player_[player].copy_flags = 6;
		break;
		case 7:
			player_[player].size = 2;
			player_[player].copy_flags = 0;
		break;
	}

	missile_[player].size = (value >> 4)&3;
}

void TIA::set_player_graphic(int player, uint8_t value)
{
	player_[player].graphic = value;
}

void TIA::set_player_reflected(int player, bool reflected)
{
	player_[player].reverse_mask = reflected ? 7 : 0;
}

void TIA::set_player_delay(int player, bool delay)
{
	// TODO
}

void TIA::set_player_position(int player)
{
	position_[(int)MotionIndex::Player0 + player] = 0;
}

void TIA::set_player_motion(int player, uint8_t motion)
{
	motion_[(int)MotionIndex::Player0 + player] = motion >> 4;
}

void TIA::set_player_missile_colour(int player, uint8_t colour)
{
	colour_palette_[(int)ColourIndex::PlayerMissile0 + player] = colour;
}

void TIA::set_missile_enable(int missile, bool enabled)
{
}

void TIA::set_missile_position(int missile)
{
}

void TIA::set_missile_position_to_player(int missile)
{
}

void TIA::set_missile_motion(int missile, uint8_t motion)
{
}

void TIA::set_ball_enable(bool enabled)
{
}

void TIA::set_ball_delay(bool delay)
{
}

void TIA::set_ball_position()
{
}

void TIA::set_ball_motion(uint8_t motion)
{
}

void TIA::move()
{
	horizontal_blank_extend_ = true;
	is_moving_[0] = is_moving_[1] = is_moving_[2] = is_moving_[3] = is_moving_[4] = true;
	horizontal_move_start_time_ = horizontal_counter_;
}

void TIA::clear_motion()
{
	motion_[0] = motion_[1] = motion_[2] = motion_[3] = motion_[4] = 0;
}

uint8_t TIA::get_collision_flags(int offset)
{
	return (uint8_t)((collision_flags_ >> (offset << 1)) << 6) & 0xc0;
}

void TIA::clear_collision_flags()
{
	collision_flags_ = 0;
}

void TIA::output_for_cycles(int number_of_cycles)
{
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

	if(!output_cursor)
	{
		memset(collision_buffer_, 0, sizeof(collision_buffer_));
		horizontal_blank_extend_ = false;
	}

	// accumulate an OR'd version of the output into the collision buffer
	draw_playfield(output_cursor, horizontal_counter_);
	draw_player(player_[0], CollisionType::Player0, (int)MotionIndex::Player0, output_cursor, horizontal_counter_);
	draw_player(player_[1], CollisionType::Player1, (int)MotionIndex::Player1, output_cursor, horizontal_counter_);

	// convert to television signals

#define Period(function, target)	\
	if(output_cursor < target) \
	{ \
		if(horizontal_counter_ <= target) \
		{ \
			crt_->function((unsigned int)((horizontal_counter_ - output_cursor) * 2)); \
			horizontal_counter_ %= cycles_per_line; \
			return; \
		} \
		else \
		{ \
			crt_->function((unsigned int)((target - output_cursor) * 2)); \
			output_cursor = target; \
		} \
	}

	switch(output_mode_)
	{
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

	if(output_mode_ & blank_flag)
	{
		if(pixel_target_)
		{
			output_pixels(pixels_start_location_, output_cursor);
			crt_->output_data((unsigned int)(output_cursor - pixels_start_location_) * 2, 2);
			pixel_target_ = nullptr;
			pixels_start_location_ = 0;
		}
		int duration = std::min(228, horizontal_counter_) - output_cursor;
		crt_->output_blank((unsigned int)(duration * 2));
	}
	else
	{
		if(!pixels_start_location_)
		{
			pixels_start_location_ = output_cursor;
			pixel_target_ = crt_->allocate_write_area(160);
		}

		// convert that into pixels
		if(pixel_target_) output_pixels(output_cursor, horizontal_counter_);

		// accumulate collision flags
		while(output_cursor < horizontal_counter_)
		{
			collision_flags_ |= collision_flags_by_buffer_vaules_[collision_buffer_[output_cursor - first_pixel_cycle]];
			output_cursor++;
		}

		if(horizontal_counter_ == cycles_per_line)
		{
			crt_->output_data((unsigned int)(output_cursor - pixels_start_location_) * 2, 2);
			pixel_target_ = nullptr;
			pixels_start_location_ = 0;
		}
	}

	horizontal_counter_ %= cycles_per_line;
}

void TIA::output_pixels(int start, int end)
{
	int target_position = start - pixels_start_location_;

	if(start < first_pixel_cycle+8 && horizontal_blank_extend_)
	{
		while(start < end && start < first_pixel_cycle+8)
		{
			pixel_target_[target_position] = 0;
			start++;
			target_position++;
		}
	}

	if(playfield_priority_ == PlayfieldPriority::Score)
	{
		while(start < end && start < first_pixel_cycle + 80)
		{
			uint8_t buffer_value = collision_buffer_[start - first_pixel_cycle];
			pixel_target_[target_position] = colour_palette_[colour_mask_by_mode_collision_flags_[(int)ColourMode::ScoreLeft][buffer_value]];
			start++;
			target_position++;
		}
		while(start < end)
		{
			uint8_t buffer_value = collision_buffer_[start - first_pixel_cycle];
			pixel_target_[target_position] = colour_palette_[colour_mask_by_mode_collision_flags_[(int)ColourMode::ScoreRight][buffer_value]];
			start++;
			target_position++;
		}
	}
	else
	{
		int table_index = (int)((playfield_priority_ == PlayfieldPriority::Standard) ? ColourMode::Standard : ColourMode::OnTop);
		while(start < end)
		{
			uint8_t buffer_value = collision_buffer_[start - first_pixel_cycle];
			pixel_target_[target_position] = colour_palette_[colour_mask_by_mode_collision_flags_[table_index][buffer_value]];
			start++;
			target_position++;
		}
	}
}

void TIA::output_line()
{
	switch(output_mode_)
	{
		default:
			// TODO: optimise special case
			output_for_cycles(cycles_per_line);
		break;
		case sync_flag:
		case sync_flag | blank_flag:
			crt_->output_sync(32);
			crt_->output_blank(32);
			crt_->output_sync(392);
			horizontal_blank_extend_ = false;
		break;
		case blank_flag:
			crt_->output_blank(32);
			crt_->output_sync(32);
			crt_->output_default_colour_burst(32);
			crt_->output_blank(360);
			horizontal_blank_extend_ = false;
		break;
	}
}

#pragma mark - Playfield output

void TIA::draw_playfield(int start, int end)
{
	// don't do anything if this window ends too early
	if(end < first_pixel_cycle - 4) return;

	// look at what needs to be output four cycles into the future, to model playfield output latency
	start += 4;
	end += 4;

	// clip to drawable bounds
	start = std::max(start, first_pixel_cycle);
	end = std::min(end, 228);

	// proceed along four-pixel boundaries, plotting four pixels at a time
	int aligned_position = (start + 3)&~3;
	while(aligned_position < end)
	{
		int offset = (aligned_position - first_pixel_cycle) >> 2;
		uint32_t value = ((background_[(offset/20)&background_half_mask_] >> (offset%20))&1) * 0x01010101;
		*(uint32_t *)&collision_buffer_[aligned_position - first_pixel_cycle] |= value;
		aligned_position += 4;
	}
}

#pragma mark - Player output

void TIA::draw_player(Player &player, CollisionType collision_identity, const int position_identity, int start, int end)
{
	int &position = position_[position_identity];
	int &motion = motion_[position_identity];
	bool &is_moving = is_moving_[position_identity];

	// movement works across the entire screen, so do work that falls outside of the pixel area
	int first_pixel = first_pixel_cycle + (horizontal_blank_extend_ ? 8 : 0);
	int movement_time = (start + 3) & ~3;
	if(is_moving)
	{
		// round up to the next H@1 cycle
		while(movement_time < end && movement_time < first_pixel)
		{
			int movement = (movement_time - horizontal_move_start_time_) >> 2;
			if(movement == (motion ^ 8))
			{
				is_moving = false;
				break;
			}
			position ++;
			movement_time += 4;
		}
		position %= 160;
	}

	// don't continue to do any drawing if this window ends too early
	if(end < first_pixel) return;
	if(start < first_pixel) start = first_pixel;

	// perform a miniature event loop on (i) triggering draws; (ii) drawing; and (iii) motion
	if(is_moving || player.graphic)
	{
		while(start < end)
		{
			int next_event_time = end;

			// is the next event a movement tick?
			if(is_moving && movement_time + 4 < next_event_time)
			{
				next_event_time = movement_time + 4;
			}

			// is the next event a graphics draw?
			int next_copy = 160;
			if(player.graphic)
			{
				if(position < 16 && player.copy_flags&1)
				{
					next_copy = 16;
				} else if(position < 32 && player.copy_flags&2)
				{
					next_copy = 32;
				} else if(position < 64 && player.copy_flags&4)
				{
					next_copy = 64;
				}

				int time_until_copy = next_copy - position;
				if(start+time_until_copy < next_event_time) next_event_time = start+time_until_copy;
			}

			// the next interesting event is after next_event_time cycles, so progress
			position = (position + next_event_time) % 160;
			start = next_event_time;

			// if the event is a motion tick, apply
			if(is_moving && start == movement_time + 4)
			{
				int movement_step = (movement_time - horizontal_move_start_time_) >> 2;
				if(movement_step == (motion ^ 8))
				{
					is_moving = false;
				}
				else
				{
					position ++;
					movement_time += 4;
				}
			}

			// if it's a draw trigger, draw (TODO: use the actual graphic)
			if(position == (next_copy % 160))
			{
				int cursor = start + 5 - first_pixel_cycle;
				int terminus = std::min(160, cursor+8);
				while(cursor < terminus)
				{
					collision_buffer_[cursor] |= (uint8_t)collision_identity;
					cursor++;
				}
			}
		}
	}
	else
	{
		// just advance the timer all in one jump
		position = (position + start - end) % 160;
	}
}
