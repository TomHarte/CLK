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
	const double NTSC_clock_rate = 1194720;
	const double PAL_clock_rate = 1182298;
}

TIA::TIA() :
	horizontal_counter_(0)
{
/*
	crt_.reset(new Outputs::CRT::CRT(228, 1, 263, Outputs::CRT::ColourSpace::YIQ, 228, 1, false, 1));
	crt_->set_output_device(Outputs::CRT::Television);

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
	speaker_->set_input_rate((float)(get_clock_rate() / 38.0));*/
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
		horizontal_counter_ = (horizontal_counter_ + cycles) % cycles_per_line;
		number_of_cycles -= cycles;
	}

	// output full lines for as long as possible
	while(number_of_cycles > cycles_per_line)
	{
		output_line();
		number_of_cycles -= cycles_per_line;
	}

	// partly start a new line if necessary
	if(number_of_cycles)
	{
		output_for_cycles(number_of_cycles);
		horizontal_counter_ = (horizontal_counter_ + number_of_cycles) % cycles_per_line;
	}
}

void TIA::set_vsync(bool vsync)
{
}

void TIA::set_vblank(bool vblank)
{
}

void TIA::reset_horizontal_counter()
{
}

int TIA::get_cycles_until_horizontal_blank(unsigned int from_offset)
{
	return cycles_per_line - (horizontal_counter_ + (int)from_offset) % cycles_per_line;
}

void TIA::set_background_colour(uint8_t colour)
{
}

void TIA::set_playfield(uint16_t offset, uint8_t value)
{
}

void TIA::set_playfield_control_and_ball_size(uint8_t value)
{
}

void TIA::set_playfield_ball_colour(uint8_t colour)
{
}

void TIA::set_player_number_and_size(int player, uint8_t value)
{
}

void TIA::set_player_graphic(int player, uint8_t value)
{
}

void TIA::set_player_reflected(int player, bool reflected)
{
}

void TIA::set_player_delay(int player, bool delay)
{
}

void TIA::set_player_position(int player)
{
}

void TIA::set_player_motion(int player, uint8_t motion)
{
}

void TIA::set_player_missile_colour(int player, uint8_t colour)
{
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
}

void TIA::clear_motion()
{
}

uint8_t TIA::get_collision_flags(int offset)
{
	return 0xff;
}

void TIA::clear_collision_flags()
{
}

void TIA::output_for_cycles(int number_of_cycles)
{
}

void TIA::output_line()
{
}
