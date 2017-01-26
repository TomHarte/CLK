//
//  Atari2600.cpp
//  CLK
//
//  Created by Thomas Harte on 14/07/2015.
//  Copyright © 2015 Thomas Harte. All rights reserved.
//

#include "Atari2600.hpp"
#include <algorithm>
#include <stdio.h>

using namespace Atari2600;
namespace {
	static const unsigned int horizontalTimerPeriod = 228;
	static const double NTSC_clock_rate = 1194720;
	static const double PAL_clock_rate = 1182298;
}

Machine::Machine() :
	horizontal_timer_(0),
	last_output_state_duration_(0),
	last_output_state_(OutputState::Sync),
	rom_(nullptr),
	tia_input_value_{0xff, 0xff},
	upcoming_events_pointer_(0),
	object_counter_pointer_(0),
	state_by_time_(state_by_extend_time_[0]),
	cycles_since_speaker_update_(0),
	is_pal_region_(false)
{
	memset(collisions_, 0xff, sizeof(collisions_));
	setup_reported_collisions();

	for(int vbextend = 0; vbextend < 2; vbextend++)
	{
		for(int c = 0; c < 57; c++)
		{
			OutputState state;

			// determine which output state will be active in four cycles from now
			switch(c)
			{
				case 0: case 1: case 2: case 3:					state = OutputState::Blank;									break;
				case 4: case 5: case 6: case 7:					state = OutputState::Sync;									break;
				case 8: case 9: case 10: case 11:				state = OutputState::ColourBurst;							break;
				case 12: case 13: case 14:
				case 15: case 16:								state = OutputState::Blank;									break;

				case 17: case 18:								state = vbextend ? OutputState::Blank : OutputState::Pixel;	break;
				default:										state = OutputState::Pixel;									break;
			}

			state_by_extend_time_[vbextend][c] = state;
		}
	}
	set_clock_rate(NTSC_clock_rate);
}

void Machine::setup_output(float aspect_ratio)
{
	speaker_.reset(new Speaker);
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
	speaker_->set_input_rate((float)(get_clock_rate() / 38.0));
}

void Machine::switch_region()
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
}

void Machine::close_output()
{
	crt_ = nullptr;
}

Machine::~Machine()
{
	delete[] rom_;
	close_output();
}

void Machine::update_timers(int mask)
{
	unsigned int upcoming_pointer_plus_4 = (upcoming_events_pointer_ + 4)%number_of_upcoming_events;

	object_counter_pointer_ = (object_counter_pointer_ + 1)%number_of_recorded_counters;
	ObjectCounter *oneClockAgo = object_counter_[(object_counter_pointer_ - 1 + number_of_recorded_counters)%number_of_recorded_counters];
	ObjectCounter *twoClocksAgo = object_counter_[(object_counter_pointer_ - 2 + number_of_recorded_counters)%number_of_recorded_counters];
	ObjectCounter *now = object_counter_[object_counter_pointer_];

	// grab the background now, for application in four clocks
	if(mask & (1 << 5) && !(horizontal_timer_&3))
	{
		unsigned int offset = 4 + horizontal_timer_ - (horizontalTimerPeriod - 160);
		upcoming_events_[upcoming_pointer_plus_4].updates |= Event::Action::Playfield;
		upcoming_events_[upcoming_pointer_plus_4].playfield_pixel = playfield_[(offset >> 2)%40];
	}

	if(mask & (1 << 4))
	{
		// the ball becomes visible whenever it hits zero, regardless of whether its status
		// is the result of a counter rollover or a programmatic reset, and there's a four
		// clock delay on that triggering the start signal
		now[4].count = (oneClockAgo[4].count + 1)%160;
		now[4].pixel = oneClockAgo[4].pixel + 1;
		if(!now[4].count) now[4].pixel = 0;
	}
	else
	{
		now[4] = oneClockAgo[4];
	}

	// check for player and missle triggers
	for(int c = 0; c < 4; c++)
	{
		if(mask & (1 << c))
		{
			// update the count
			now[c].count = (oneClockAgo[c].count + 1)%160;

			uint8_t repeatMask = player_and_missile_size_[c&1] & 7;
			ObjectCounter *rollover;
			ObjectCounter *equality;

			if(c < 2)
			{
				// update the pixel
				now[c].broad_pixel = oneClockAgo[c].broad_pixel + 1;
				switch(repeatMask)
				{
					default:	now[c].pixel = oneClockAgo[c].pixel + 1;	break;
					case 5:		now[c].pixel = oneClockAgo[c].pixel + (now[c].broad_pixel&1);	break;
					case 7:		now[c].pixel = oneClockAgo[c].pixel + (((now[c].broad_pixel | (now[c].broad_pixel >> 1))^1)&1);	break;
				}

				// check for a rollover six clocks ago or equality five clocks ago
				rollover = twoClocksAgo;
				equality = oneClockAgo;
			}
			else
			{
				// update the pixel
				now[c].pixel = oneClockAgo[c].pixel + 1;

				// check for a rollover five clocks ago or equality four clocks ago
				rollover = oneClockAgo;
				equality = now;
			}

			if(
				(rollover[c].count == 159) ||
				(has_second_copy_[c&1] && equality[c].count == 16) ||
				(has_third_copy_[c&1] && equality[c].count == 32) ||
				(has_fourth_copy_[c&1] && equality[c].count == 64)
			)
			{
				now[c].pixel = 0;
				now[c].broad_pixel = 0;
			}
		}
		else
		{
			now[c] = oneClockAgo[c];
		}
	}
}

uint8_t Machine::get_output_pixel()
{
	ObjectCounter *now = object_counter_[object_counter_pointer_];

	// get the playfield pixel
	unsigned int offset = horizontal_timer_ - (horizontalTimerPeriod - 160);
	uint8_t playfieldColour = ((playfield_control_&6) == 2) ? player_colour_[offset / 80] : playfield_colour_;

	// ball pixel
	uint8_t ballPixel = 0;
	if(now[4].pixel < ball_size_) {
		ballPixel = ball_graphics_enable_[ball_graphics_selector_];
	}

	// determine the player and missile pixels
	uint8_t playerPixels[2] = { 0, 0 };
	uint8_t missilePixels[2] = { 0, 0 };
	for(int c = 0; c < 2; c++)
	{
		if(player_graphics_[c] && now[c].pixel < 8) {
			playerPixels[c] = (player_graphics_[player_graphics_selector_[c]][c] >> (now[c].pixel ^ player_reflection_mask_[c])) & 1;
		}

		if(!missile_graphics_reset_[c] && now[c+2].pixel < missile_size_[c]) {
			missilePixels[c] = missile_graphics_enable_[c];
		}
	}

	// accumulate collisions
	int pixel_mask = playerPixels[0] | (playerPixels[1] << 1) | (missilePixels[0] << 2) | (missilePixels[1] << 3) | (ballPixel << 4) | (playfield_output_ << 5);
	collisions_[0] |= reported_collisions_[pixel_mask][0];
	collisions_[1] |= reported_collisions_[pixel_mask][1];
	collisions_[2] |= reported_collisions_[pixel_mask][2];
	collisions_[3] |= reported_collisions_[pixel_mask][3];
	collisions_[4] |= reported_collisions_[pixel_mask][4];
	collisions_[5] |= reported_collisions_[pixel_mask][5];
	collisions_[6] |= reported_collisions_[pixel_mask][6];
	collisions_[7] |= reported_collisions_[pixel_mask][7];

	// apply appropriate priority to pick a colour
	uint8_t playfield_pixel = playfield_output_ | ballPixel;
	uint8_t outputColour = playfield_pixel ? playfieldColour : background_colour_;

	if(!(playfield_control_&0x04) || !playfield_pixel) {
		if(playerPixels[1] || missilePixels[1]) outputColour = player_colour_[1];
		if(playerPixels[0] || missilePixels[0]) outputColour = player_colour_[0];
	}

	// return colour
	return outputColour;
}

void Machine::setup_reported_collisions()
{
	for(int c = 0; c < 64; c++)
	{
		memset(reported_collisions_[c], 0, 8);

		int playerPixels[2] = { c&1, (c >> 1)&1 };
		int missilePixels[2] = { (c >> 2)&1, (c >> 3)&1 };
		int ballPixel = (c >> 4)&1;
		int playfield_pixel = (c >> 5)&1;

		if(playerPixels[0] | playerPixels[1]) {
			reported_collisions_[c][0] |= ((missilePixels[0] & playerPixels[1]) << 7)	| ((missilePixels[0] & playerPixels[0]) << 6);
			reported_collisions_[c][1] |= ((missilePixels[1] & playerPixels[0]) << 7)	| ((missilePixels[1] & playerPixels[1]) << 6);

			reported_collisions_[c][2] |= ((playfield_pixel & playerPixels[0]) << 7)	| ((ballPixel & playerPixels[0]) << 6);
			reported_collisions_[c][3] |= ((playfield_pixel & playerPixels[1]) << 7)	| ((ballPixel & playerPixels[1]) << 6);

			reported_collisions_[c][7] |= ((playerPixels[0] & playerPixels[1]) << 7);
		}

		if(playfield_pixel | ballPixel) {
			reported_collisions_[c][4] |= ((playfield_pixel & missilePixels[0]) << 7)	| ((ballPixel & missilePixels[0]) << 6);
			reported_collisions_[c][5] |= ((playfield_pixel & missilePixels[1]) << 7)	| ((ballPixel & missilePixels[1]) << 6);

			reported_collisions_[c][6] |= ((playfield_pixel & ballPixel) << 7);
		}

		if(missilePixels[0] & missilePixels[1])
			reported_collisions_[c][7] |= (1 << 6);
	}
}

void Machine::output_pixels(unsigned int count)
{
	while(count--)
	{
		if(upcoming_events_[upcoming_events_pointer_].updates)
		{
			// apply any queued changes and flush the record
			if(upcoming_events_[upcoming_events_pointer_].updates & Event::Action::HMoveSetup)
			{
				// schedule an extended left border
				state_by_time_ = state_by_extend_time_[1];

				// clear any ongoing moves
				if(hmove_flags_)
				{
					for(int c = 0; c < number_of_upcoming_events; c++)
					{
						upcoming_events_[c].updates &= ~(Event::Action::HMoveCompare | Event::Action::HMoveDecrement);
					}
				}

				// schedule new moves
				hmove_flags_ = 0x1f;
				hmove_counter_ = 15;

				// follow-through into a compare immediately
				upcoming_events_[upcoming_events_pointer_].updates |= Event::Action::HMoveCompare;
			}

			if(upcoming_events_[upcoming_events_pointer_].updates & Event::Action::HMoveCompare)
			{
				for(int c = 0; c < 5; c++)
				{
					if(((object_motions_[c] >> 4)^hmove_counter_) == 7)
					{
						hmove_flags_ &= ~(1 << c);
					}
				}
				if(hmove_flags_)
				{
					if(hmove_counter_) hmove_counter_--;
					upcoming_events_[(upcoming_events_pointer_+4)%number_of_upcoming_events].updates |= Event::Action::HMoveCompare;
					upcoming_events_[(upcoming_events_pointer_+2)%number_of_upcoming_events].updates |= Event::Action::HMoveDecrement;
				}
			}

			if(upcoming_events_[upcoming_events_pointer_].updates & Event::Action::HMoveDecrement)
			{
				update_timers(hmove_flags_);
			}

			if(upcoming_events_[upcoming_events_pointer_].updates & Event::Action::ResetCounter)
			{
				object_counter_[object_counter_pointer_][upcoming_events_[upcoming_events_pointer_].counter].count = 0;
			}

			// zero out current update event
			upcoming_events_[upcoming_events_pointer_].updates = 0;
		}

		//  progress to next event
		upcoming_events_pointer_ = (upcoming_events_pointer_ + 1)%number_of_upcoming_events;

		// determine which output state is currently active
		OutputState primary_state = state_by_time_[horizontal_timer_ >> 2];
		OutputState effective_state = primary_state;

		// update pixel timers
		if(primary_state == OutputState::Pixel) update_timers(~0);

		// update the background chain
		if(horizontal_timer_ >= 64 && horizontal_timer_ <= 160+64 && !(horizontal_timer_&3))
		{
			playfield_output_ = next_playfield_output_;
			next_playfield_output_ = playfield_[(horizontal_timer_ - 64) >> 2];
		}

		// if vsync is enabled, output the opposite of the automatic hsync output;
		// also	honour the vertical blank flag
		if(vsync_enabled_) {
			effective_state = (effective_state = OutputState::Sync) ? OutputState::Blank : OutputState::Sync;
		} else if(vblank_enabled_ && effective_state == OutputState::Pixel) {
			effective_state = OutputState::Blank;
		}

		// decide what that means needs to be communicated to the CRT
		last_output_state_duration_++;
		if(effective_state != last_output_state_) {
			switch(last_output_state_) {
				case OutputState::Blank:		crt_->output_blank(last_output_state_duration_);				break;
				case OutputState::Sync:			crt_->output_sync(last_output_state_duration_);					break;
				case OutputState::ColourBurst:	crt_->output_colour_burst(last_output_state_duration_, 96, 0);	break;
				case OutputState::Pixel:		crt_->output_data(last_output_state_duration_,	1);				break;
			}
			last_output_state_duration_ = 0;
			last_output_state_ = effective_state;

			if(effective_state == OutputState::Pixel) {
				output_buffer_ = crt_->allocate_write_area(160);
			} else {
				output_buffer_ = nullptr;
			}
		}

		// decide on a pixel colour if that's what's happening
		if(effective_state == OutputState::Pixel)
		{
			uint8_t colour = get_output_pixel();
			if(output_buffer_)
			{
				*output_buffer_ = colour;
				output_buffer_++;
			}
		}

		// advance horizontal timer, perform reset actions if desired
		horizontal_timer_ = (horizontal_timer_ + 1) % horizontalTimerPeriod;
		if(!horizontal_timer_)
		{
			// switch back to a normal length left border
			state_by_time_ = state_by_extend_time_[0];
			set_ready_line(false);
		}
	}
}

unsigned int Machine::perform_bus_operation(CPU6502::BusOperation operation, uint16_t address, uint8_t *value)
{
	uint8_t returnValue = 0xff;
	unsigned int cycles_run_for = 3;

	// this occurs as a feedback loop — the 2600 requests ready, then performs the cycles_run_for
	// leap to the end of ready only once ready is signalled — because on a 6502 ready doesn't take
	// effect until the next read; therefore it isn't safe to assume that signalling ready immediately
	// skips to the end of the line.
	if(operation == CPU6502::BusOperation::Ready) {
		unsigned int distance_to_end_of_ready = horizontalTimerPeriod - horizontal_timer_;
		cycles_run_for = distance_to_end_of_ready;
	}

	output_pixels(cycles_run_for);
	cycles_since_speaker_update_ += cycles_run_for;

	if(operation != CPU6502::BusOperation::Ready) {

		// check for a paging access
		if(rom_size_ > 4096 && ((address & 0x1f00) == 0x1f00)) {
			uint8_t *base_ptr = rom_pages_[0];
			uint8_t first_paging_register = (uint8_t)(0xf8 - (rom_size_ >> 14)*2);

			const uint8_t paging_register = address&0xff;
			if(paging_register >= first_paging_register) {
				const uint16_t selected_page = paging_register - first_paging_register;
				if(selected_page * 4096 < rom_size_) {
					base_ptr = &rom_[selected_page * 4096];
				}
			}

			if(base_ptr != rom_pages_[0]) {
				rom_pages_[0] = base_ptr;
				rom_pages_[1] = base_ptr + 1024;
				rom_pages_[2] = base_ptr + 2048;
				rom_pages_[3] = base_ptr + 3072;
			}
		}

		// check for a ROM read
		if((address&0x1000) && isReadOperation(operation)) {
			returnValue &= rom_pages_[(address >> 10)&3][address&1023];
		}

		// check for a RAM access
		if((address&0x1280) == 0x80) {
			if(isReadOperation(operation)) {
				returnValue &= mos6532_.get_ram(address);
			} else {
				mos6532_.set_ram(address, *value);
			}
		}

		// check for a TIA access
		if(!(address&0x1080)) {
			if(isReadOperation(operation)) {
				const uint16_t decodedAddress = address & 0xf;
				switch(decodedAddress) {
					case 0x00:		// missile 0 / player collisions
					case 0x01:		// missile 1 / player collisions
					case 0x02:		// player 0 / playfield / ball collisions
					case 0x03:		// player 1 / playfield / ball collisions
					case 0x04:		// missile 0 / playfield / ball collisions
					case 0x05:		// missile 1 / playfield / ball collisions
					case 0x06:		// ball / playfield collisions
					case 0x07:		// player / player, missile / missile collisions
						returnValue &= collisions_[decodedAddress];
					break;

					case 0x08:
					case 0x09:
					case 0x0a:
					case 0x0b:
						// TODO: pot ports
					break;

					case 0x0c:
					case 0x0d:
						returnValue &= tia_input_value_[decodedAddress - 0x0c];
					break;
				}
			} else {
				const uint16_t decodedAddress = address & 0x3f;
				switch(decodedAddress) {
					case 0x00:
						vsync_enabled_ = !!(*value & 0x02);
					break;
					case 0x01:	vblank_enabled_ = !!(*value & 0x02);	break;

					case 0x02:
						if(horizontal_timer_) set_ready_line(true);
					break;
					case 0x03:
						// Reset is delayed by four cycles.
						horizontal_timer_ = horizontalTimerPeriod - 4;

						// TODO: audio will now be out of synchronisation — fix
					break;

					case 0x04:
					case 0x05: {
						int entry = decodedAddress - 0x04;
						player_and_missile_size_[entry] = *value;
						missile_size_[entry] = 1 << ((*value >> 4)&3);

						uint8_t repeatMask = (*value)&7;
						has_second_copy_[entry] = (repeatMask == 1) || (repeatMask == 3);
						has_third_copy_[entry] = (repeatMask == 2) || (repeatMask == 3) || (repeatMask == 6);
						has_fourth_copy_[entry] = (repeatMask == 4) || (repeatMask == 6);
					} break;

					case 0x06:
					case 0x07: player_colour_[decodedAddress - 0x06] = *value;	break;
					case 0x08: playfield_colour_ = *value;	break;
					case 0x09: background_colour_ = *value;	break;

					case 0x0a: {
						uint8_t old_playfield_control = playfield_control_;
						playfield_control_ = *value;
						ball_size_ = 1 << ((playfield_control_ >> 4)&3);

						// did the mirroring bit change?
						if((playfield_control_^old_playfield_control)&1) {
							if(playfield_control_&1) {
								for(int c = 0; c < 20; c++) playfield_[c+20] = playfield_[19-c];
							} else {
								memcpy(&playfield_[20], playfield_, 20);
							}
						}
					} break;
					case 0x0b:
					case 0x0c: player_reflection_mask_[decodedAddress - 0x0b] = (*value)&8 ? 0 : 7;	break;

					case 0x0d:
						playfield_[0] = ((*value) >> 4)&1;
						playfield_[1] = ((*value) >> 5)&1;
						playfield_[2] = ((*value) >> 6)&1;
						playfield_[3] = (*value) >> 7;

						if(playfield_control_&1) {
							for(int c = 0; c < 4; c++) playfield_[39-c] = playfield_[c];
						} else {
							memcpy(&playfield_[20], playfield_, 4);
						}
					break;
					case 0x0e:
						playfield_[4] = (*value) >> 7;
						playfield_[5] = ((*value) >> 6)&1;
						playfield_[6] = ((*value) >> 5)&1;
						playfield_[7] = ((*value) >> 4)&1;
						playfield_[8] = ((*value) >> 3)&1;
						playfield_[9] = ((*value) >> 2)&1;
						playfield_[10] = ((*value) >> 1)&1;
						playfield_[11] = (*value)&1;

						if(playfield_control_&1) {
							for(int c = 0; c < 8; c++) playfield_[35-c] = playfield_[c+4];
						} else {
							memcpy(&playfield_[24], &playfield_[4], 8);
						}
					break;
					case 0x0f:
						playfield_[19] = (*value) >> 7;
						playfield_[18] = ((*value) >> 6)&1;
						playfield_[17] = ((*value) >> 5)&1;
						playfield_[16] = ((*value) >> 4)&1;
						playfield_[15] = ((*value) >> 3)&1;
						playfield_[14] = ((*value) >> 2)&1;
						playfield_[13] = ((*value) >> 1)&1;
						playfield_[12] = (*value)&1;

						if(playfield_control_&1) {
							for(int c = 0; c < 8; c++) playfield_[27-c] = playfield_[c+12];
						} else {
							memcpy(&playfield_[32], &playfield_[12], 8);
						}
					break;

					case 0x10:	case 0x11:	case 0x12:	case 0x13:
					case 0x14:
						upcoming_events_[(upcoming_events_pointer_ + 4)%number_of_upcoming_events].updates |= Event::Action::ResetCounter;
						upcoming_events_[(upcoming_events_pointer_ + 4)%number_of_upcoming_events].counter = decodedAddress - 0x10;
					break;

					case 0x15: case 0x16:
						update_audio();
						speaker_->set_control(decodedAddress - 0x15, *value);
					break;

					case 0x17: case 0x18:
						update_audio();
						speaker_->set_divider(decodedAddress - 0x17, *value);
					break;

					case 0x19: case 0x1a:
						update_audio();
						speaker_->set_volume(decodedAddress - 0x19, *value);
					break;

					case 0x1c:
						ball_graphics_enable_[1] = ball_graphics_enable_[0];
					case 0x1b: {
						int index = decodedAddress - 0x1b;
						player_graphics_[0][index] = *value;
						player_graphics_[1][index^1] = player_graphics_[0][index^1];
					} break;
					case 0x1d:
					case 0x1e:
						missile_graphics_enable_[decodedAddress - 0x1d] = ((*value) >> 1)&1;
//						printf("e:%02x <- %c\n", decodedAddress - 0x1d, ((*value)&1) ? 'E' : '-');
					break;
					case 0x1f:
						ball_graphics_enable_[0] = ((*value) >> 1)&1;
					break;

					case 0x20:
					case 0x21:
					case 0x22:
					case 0x23:
					case 0x24:
						object_motions_[decodedAddress - 0x20] = *value;
					break;

					case 0x25: player_graphics_selector_[0]	= (*value)&1;	break;
					case 0x26: player_graphics_selector_[1]	= (*value)&1;	break;
					case 0x27: ball_graphics_selector_		= (*value)&1;	break;

					case 0x28:
					case 0x29:
					{
						// TODO: this should properly mean setting a flag and propagating later, I think?
						int index = decodedAddress - 0x28;
						if(!(*value&0x02) && missile_graphics_reset_[index])
						{
							object_counter_[object_counter_pointer_][index + 2].count = object_counter_[object_counter_pointer_][index].count;

							uint8_t repeatMask = player_and_missile_size_[index] & 7;
							int extra_offset;
							switch(repeatMask)
							{
								default:	extra_offset = 3;	break;
								case 5:		extra_offset = 6;	break;
								case 7:		extra_offset = 10;	break;
							}

							object_counter_[object_counter_pointer_][index + 2].count = (object_counter_[object_counter_pointer_][index + 2].count + extra_offset)%160;
						}
						missile_graphics_reset_[index] = !!((*value) & 0x02);
//						printf("r:%02x <- %c\n", decodedAddress - 0x28, ((*value)&2) ? 'R' : '-');
					}
					break;

					case 0x2a: {
						// justification for +5: "we need to wait at least 71 [clocks] before the HMOVE operation is complete";
						// which will take 16*4 + 2 = 66 cycles from the first compare, implying the first compare must be
						// in five cycles from now
//						int start_pause = ((horizontal_timer_ + 3)&3) + 4;
						upcoming_events_[(upcoming_events_pointer_ + 5)%number_of_upcoming_events].updates |= Event::Action::HMoveSetup;
					} break;
					case 0x2b:
						object_motions_[0] =
						object_motions_[1] =
						object_motions_[2] =
						object_motions_[3] =
						object_motions_[4] = 0;
					break;
					case 0x2c:
						collisions_[0] = collisions_[1] = collisions_[2] = 
						collisions_[3] = collisions_[4] = collisions_[5] = 0x3f;
						collisions_[6] = 0x7f;
						collisions_[7] = 0x3f;
					break;
				}
			}
		}

		// check for a PIA access
		if((address&0x1280) == 0x280) {
			if(isReadOperation(operation)) {
				returnValue &= mos6532_.get_register(address);
			} else {
				mos6532_.set_register(address, *value);
			}
		}

		if(isReadOperation(operation)) {
			*value = returnValue;
		}
	}

	mos6532_.run_for_cycles(cycles_run_for / 3);

	return cycles_run_for / 3;
}

void Machine::set_digital_input(Atari2600DigitalInput input, bool state)
{
	switch (input) {
		case Atari2600DigitalInputJoy1Up:		mos6532_.update_port_input(0, 0x10, state);	break;
		case Atari2600DigitalInputJoy1Down:		mos6532_.update_port_input(0, 0x20, state);	break;
		case Atari2600DigitalInputJoy1Left:		mos6532_.update_port_input(0, 0x40, state);	break;
		case Atari2600DigitalInputJoy1Right:	mos6532_.update_port_input(0, 0x80, state);	break;

		case Atari2600DigitalInputJoy2Up:		mos6532_.update_port_input(0, 0x01, state);	break;
		case Atari2600DigitalInputJoy2Down:		mos6532_.update_port_input(0, 0x02, state);	break;
		case Atari2600DigitalInputJoy2Left:		mos6532_.update_port_input(0, 0x04, state);	break;
		case Atari2600DigitalInputJoy2Right:	mos6532_.update_port_input(0, 0x08, state);	break;

		// TODO: latching
		case Atari2600DigitalInputJoy1Fire:		if(state) tia_input_value_[0] &= ~0x80; else tia_input_value_[0] |= 0x80; break;
		case Atari2600DigitalInputJoy2Fire:		if(state) tia_input_value_[1] &= ~0x80; else tia_input_value_[1] |= 0x80; break;

		default: break;
	}
}

void Machine::set_switch_is_enabled(Atari2600Switch input, bool state)
{
	switch(input) {
		case Atari2600SwitchReset:					mos6532_.update_port_input(1, 0x01, state);	break;
		case Atari2600SwitchSelect:					mos6532_.update_port_input(1, 0x02, state);	break;
		case Atari2600SwitchColour:					mos6532_.update_port_input(1, 0x08, state);	break;
		case Atari2600SwitchLeftPlayerDifficulty:	mos6532_.update_port_input(1, 0x40, state);	break;
		case Atari2600SwitchRightPlayerDifficulty:	mos6532_.update_port_input(1, 0x80, state);	break;
	}
}

void Machine::configure_as_target(const StaticAnalyser::Target &target)
{
	if(!target.cartridges.front()->get_segments().size()) return;
	Storage::Cartridge::Cartridge::Segment segment = target.cartridges.front()->get_segments().front();
	size_t length = segment.data.size();

	rom_size_ = 1024;
	while(rom_size_ < length && rom_size_ < 32768) rom_size_ <<= 1;

	delete[] rom_;
	rom_ = new uint8_t[rom_size_];

	size_t offset = 0;
	const size_t copy_step = std::min(rom_size_, length);
	while(offset < rom_size_)
	{
		size_t copy_length = std::min(copy_step, rom_size_ - offset);
		memcpy(&rom_[offset], &segment.data[0], copy_length);
		offset += copy_length;
	}

	size_t romMask = rom_size_ - 1;
	rom_pages_[0] = rom_;
	rom_pages_[1] = &rom_[1024 & romMask];
	rom_pages_[2] = &rom_[2048 & romMask];
	rom_pages_[3] = &rom_[3072 & romMask];
}

#pragma mark - Audio

void Machine::update_audio()
{
	unsigned int audio_cycles = cycles_since_speaker_update_ / 114;

	speaker_->run_for_cycles(audio_cycles);
	cycles_since_speaker_update_ %= 114;
}

void Machine::synchronise()
{
	update_audio();
	speaker_->flush();
}

