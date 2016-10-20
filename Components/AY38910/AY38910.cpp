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
	_envelope_divider(0), _envelope_period(0)
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
}

void AY38910::set_clock_rate(double clock_rate)
{
	set_input_rate((float)clock_rate);
}

void AY38910::get_samples(unsigned int number_of_samples, int16_t *target)
{
	for(int c = 0; c < number_of_samples; c++)
	{
		// a master divider divides the clock by 16
		int former_master_divider = _master_divider;
		_master_divider++;
		int resulting_steps = ((_master_divider ^ former_master_divider) >> 4) & 1;

		// from that the three channels count down
		int did_underflow;
#define step(c)	\
	_channel_dividers[c] -= resulting_steps;	\
	did_underflow = (_channel_dividers[c] >> 15)&1; \
	_channel_output[c] ^= did_underflow;	\
	_channel_dividers[c] = did_underflow * _tone_generator_controls[c] + (did_underflow^1) * _channel_dividers[c];

		step(0);
		step(1);
		step(2);

#undef step

		// ... as does the noise generator
		_noise_divider -= resulting_steps;
		did_underflow = (_noise_divider >> 15)&1;
		_noise_divider = did_underflow * (_output_registers[6]&0x1f) + (did_underflow^1) * _noise_divider;
		_noise_output ^= did_underflow&_noise_shift_register&1;
		_noise_shift_register |= ((_noise_shift_register ^ (_noise_shift_register >> 3))&1) << 17;
		_noise_shift_register >>= did_underflow;

		// ... and the envelope generator
		_envelope_divider -= resulting_steps;
		did_underflow = (_envelope_divider >> 15)&1;
		_envelope_divider = did_underflow * _envelope_period + (did_underflow^1) * _envelope_divider;
		_envelope_position += did_underflow;

//		if(_output_registers[13] == 13)
//		{
//			printf("[%d] %d", _envelope_divider, _envelope_position);
//		}

		int refill = _envelope_overflow_masks[_output_registers[13]] * (_envelope_position >> 5);
		_envelope_position = (_envelope_position & 0x1f) | refill;

		int envelope_volume = _envelope_shapes[_output_registers[13]][_envelope_position & 0xf];

//		if(_output_registers[13] == 13)
//		{
//			printf(": %d\n", envelope_volume);
//		}

		int channel_levels[3] = {
			(((((_output_registers[7] >> 0)&1)^1) & _channel_output[0]) | ((((_output_registers[7] >> 1)&1)^1) & _noise_output)) ^ 1,
			(((((_output_registers[7] >> 2)&1)^1) & _channel_output[1]) | ((((_output_registers[7] >> 3)&1)^1) & _noise_output)) ^ 1,
			(((((_output_registers[7] >> 4)&1)^1) & _channel_output[2]) | ((((_output_registers[7] >> 5)&1)^1) & _noise_output)) ^ 1,
		};

		int volumes[3] = {
			((_output_registers[8] >> 4)&1) * envelope_volume + (((_output_registers[8] >> 4)&1)^1) * (_output_registers[8]&0x1f),
			((_output_registers[9] >> 4)&1) * envelope_volume + (((_output_registers[9] >> 4)&1)^1) * (_output_registers[9]&0x1f),
			((_output_registers[10] >> 4)&1) * envelope_volume + (((_output_registers[10] >> 4)&1)^1) * (_output_registers[10]&0x1f),
		};

		target[c] = (int16_t)((
			volumes[0] * channel_levels[0] +
			volumes[1] * channel_levels[1] +
			volumes[2] * channel_levels[2]
		) * 512);
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
//					printf("envelope %d\n", masked_value);
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
		default:			new_state = Inactive;		break;

		case (int)(BCDIR | BC2 | BC1):
		case BCDIR:
		case BC1:			new_state = LatchAddress;	break;

		case (int)(BC2 | BC1):		new_state = Read;			break;
		case (int)(BCDIR | BC2):	new_state = Write;			break;
	}

	if(new_state != _control_state)
	{
		_control_state = new_state;
		switch(new_state)
		{
			default: break;
			case LatchAddress: select_register(_data_input);	break;
			case Write: set_register_value(_data_input);		break;
			case Read: _data_output = get_register_value();		break;
		}
	}
}
