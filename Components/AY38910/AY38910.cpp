//
//  AY-3-8910.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 14/10/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#include "AY38910.hpp"

using namespace GI;

AY38910::AY38910() :
	_selected_register(0),
	_channel_output{0, 0, 0}, _channel_dividers{0, 0, 0}, _tone_generator_controls{0, 0, 0},
	_noise_shift_register(0xffff), _noise_divider(0), _noise_output(0),
	_envelope_divider(0), _envelope_period(0), _envelope_position(0),
	_output_registers{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}
{
	_output_registers[8] = _output_registers[9] = _output_registers[10] = 0;

	// set up envelope lookup tables
	for(int c = 0; c < 16; c++)
	{
		for(int p = 0; p < 32; p++)
		{
			switch(c)
			{
				case 0: case 1: case 2: case 3: case 9:
					_envelope_shapes[c][p] = (p < 16) ? (p^0xf) : 0;
					_envelope_overflow_masks[c] = 0x1f;
				break;
				case 4: case 5: case 6: case 7: case 15:
					_envelope_shapes[c][p] = (p < 16) ? p : 0;
					_envelope_overflow_masks[c] = 0x1f;
				break;

				case 8:
					_envelope_shapes[c][p] = (p & 0xf) ^ 0xf;
					_envelope_overflow_masks[c] = 0x00;
				break;
				case 12:
					_envelope_shapes[c][p] = (p & 0xf);
					_envelope_overflow_masks[c] = 0x00;
				break;

				case 10:
					_envelope_shapes[c][p] = (p & 0xf) ^ ((p < 16) ? 0xf : 0x0);
					_envelope_overflow_masks[c] = 0x00;
				break;
				case 14:
					_envelope_shapes[c][p] = (p & 0xf) ^ ((p < 16) ? 0x0 : 0xf);
					_envelope_overflow_masks[c] = 0x00;
				break;

				case 11:
					_envelope_shapes[c][p] = (p < 16) ? (p^0xf) : 0xf;
					_envelope_overflow_masks[c] = 0x1f;
				break;
				case 13:
					_envelope_shapes[c][p] = (p < 16) ? p : 0xf;
					_envelope_overflow_masks[c] = 0x1f;
				break;
			}
		}
	}

	// set up volume lookup table
	float max_volume = 8192;
	float root_two = sqrtf(2.0f);
	for(int v = 0; v < 16; v++)
	{
		_volumes[v] = (int)(max_volume / powf(root_two, (float)(v ^ 0xf)));
	}
	_volumes[0] = 0;
}

void AY38910::set_clock_rate(double clock_rate)
{
	set_input_rate((float)clock_rate);
}

void AY38910::get_samples(unsigned int number_of_samples, int16_t *target)
{
	for(int c = 0; c < number_of_samples; c++)
	{
		// a master divider divides the clock by 16;
		// resulting_steps will be 1 if a tick occurred, 0 otherwise
		int former_master_divider = _master_divider;
		_master_divider++;
		int resulting_steps = ((_master_divider ^ former_master_divider) >> 4) & 1;

		// Bluffer's guide to the stuff below: I wanted to avoid branches. If I avoid branches then
		// I avoid stalls.
		//
		// Repeating patterns are:
		//	(1) decrement, then shift a high-order bit right and mask to get 1 for did underflow, 0 otherwise;
		//	(2) did_underflow * a + (did_underflow ^ 1) * b to pick between reloading and not reloading
		int did_underflow;
#define shift(x, r, steps) \
	x -= steps;	\
	did_underflow = (x >> 16)&1; \
	x = did_underflow * r + (did_underflow^1) * x;

#define step_channel(c)	\
	shift(_channel_dividers[c], _tone_generator_controls[c], resulting_steps);	\
	_channel_output[c] ^= did_underflow;

		// update the tone channels
		step_channel(0);
		step_channel(1);
		step_channel(2);

		// ... the noise generator. This recomputes the new bit repeatedly but harmlessly, only shifting
		// it into the official 17 upon divider underflow.
		shift(_noise_divider, _output_registers[6]&0x1f, resulting_steps);
		_noise_output ^= did_underflow&_noise_shift_register&1;
		_noise_shift_register |= ((_noise_shift_register ^ (_noise_shift_register >> 3))&1) << 17;
		_noise_shift_register >>= did_underflow;

		// ... and the envelope generator. Table based for pattern lookup, with a 'refill' step — a way of
		// implementing non-repeating patterns by locking them to table position 0x1f.
//		int envelope_divider = ((_master_divider ^ former_master_divider) >> 8) & 1;
		shift(_envelope_divider, _envelope_period, resulting_steps);
		_envelope_position += did_underflow;
		int refill = _envelope_overflow_masks[_output_registers[13]] * (_envelope_position >> 5);
		_envelope_position = (_envelope_position & 0x1f) | refill;
		int envelope_volume = _envelope_shapes[_output_registers[13]][_envelope_position];

#undef step_channel
#undef shift

		// The output level for a channel is:
		//	1 if neither tone nor noise is enabled;
		//	0 if either tone or noise is enabled and its value is low.
		// (which is implemented here with reverse logic, assuming _channel_output and _noise_output are already inverted)
#define level(c, tb, nb)	\
	(((((_output_registers[7] >> tb)&1)^1) & _channel_output[c]) | ((((_output_registers[7] >> nb)&1)^1) & _noise_output)) ^ 1

		int channel_levels[3] = {
			level(0, 0, 3),
			level(1, 1, 4),
			level(2, 2, 5),
		};
#undef level

		// Channel volume is a simple selection: if the bit at 0x10 is set, use the envelope volume; otherwise use the lower four bits
#define channel_volume(c)	\
	((_output_registers[c] >> 4)&1) * envelope_volume + (((_output_registers[c] >> 4)&1)^1) * (_output_registers[c]&0xf)

		int volumes[3] = {
			channel_volume(8),
			channel_volume(9),
			channel_volume(10)
		};
#undef channel_volume

		// Mix additively. TODO: non-linear volume.
		target[c] = (int16_t)(
			_volumes[volumes[0]] * channel_levels[0] +
			_volumes[volumes[1]] * channel_levels[1] +
			_volumes[volumes[2]] * channel_levels[2]
		);
	}
}

void AY38910::skip_samples(unsigned int number_of_samples)
{
	// TODO
//	printf("Skip %d\n", number_of_samples);
}

void AY38910::select_register(uint8_t r)
{
	_selected_register = r & 0xf;
}

void AY38910::set_register_value(uint8_t value)
{
	_registers[_selected_register] = value;
	if(_selected_register < 14)
	{
		int selected_register = _selected_register;
		enqueue([=] () {
			uint8_t masked_value = value;
			switch(selected_register)
			{
				case 0: case 2: case 4:
					_tone_generator_controls[selected_register >> 1] =
						(_tone_generator_controls[selected_register >> 1] & ~0xff) | value;
				break;

				case 1: case 3: case 5:
					_tone_generator_controls[selected_register >> 1] =
						(_tone_generator_controls[selected_register >> 1] & 0xff) | (uint16_t)((value&0xf) << 8);
				break;

				case 11:
					_envelope_period = (_envelope_period & ~0xff) | value;
//					printf("e: %d", _envelope_period);
				break;

				case 12:
					_envelope_period = (_envelope_period & 0xff) | (int)(value << 8);
//					printf("e: %d", _envelope_period);
				break;

				case 13:
					masked_value &= 0xf;
					_envelope_position = 0;
				break;
			}
			_output_registers[selected_register] = masked_value;
		});
	}
}

uint8_t AY38910::get_register_value()
{
	return _registers[_selected_register];
}

uint8_t AY38910::get_port_output(bool port_b)
{
	return _registers[port_b ? 15 : 14];
}

void AY38910::set_data_input(uint8_t r)
{
	_data_input = r;
}

uint8_t AY38910::get_data_output()
{
	return _data_output;
}

void AY38910::set_control_lines(ControlLines control_lines)
{
	ControlState new_state;
	switch((int)control_lines)
	{
		default:					new_state = Inactive;		break;

		case (int)(BCDIR | BC2 | BC1):
		case BCDIR:
		case BC1:					new_state = LatchAddress;	break;

		case (int)(BC2 | BC1):		new_state = Read;			break;
		case (int)(BCDIR | BC2):	new_state = Write;			break;
	}

	if(new_state != _control_state)
	{
		_control_state = new_state;
		switch(new_state)
		{
			default: break;
			case LatchAddress:	select_register(_data_input);			break;
			case Write:			set_register_value(_data_input);		break;
			case Read:			_data_output = get_register_value();	break;
		}
	}
}
