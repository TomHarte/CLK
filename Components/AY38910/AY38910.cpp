//
//  AY-3-8910.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 14/10/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#include "AY38910.hpp"

using namespace GI;

AY38910::AY38910() : _selected_register(0), _channel_ouput{0, 0, 0}, _channel_dividers{0, 0, 0}, _tone_generator_controls{0, 0, 0}
{
	_output_registers[8] = _output_registers[9] = _output_registers[10] = 0;
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
#define step(c)	{\
	_channel_dividers[c] -= resulting_steps;	\
	int did_underflow = (_channel_dividers[c] >> 15)&1; \
	_channel_ouput[c] ^= did_underflow;	\
	_channel_dividers[c] = did_underflow * _tone_generator_controls[c] + (did_underflow^1) * _channel_dividers[c]; \
	}

		step(0);
		step(1);
		step(2);

#undef step

		// ... as does the envelope generator
//		_envelope_divider -= resulting_steps;
//		if(!_envelope_divider)
//		{
//			_envelope_divider = _envelope_period;
//		}

//		if(_output_registers[9]) printf("%d %d / %d\n", _channel_ouput[1], _channel_dividers[1], _tone_generator_controls[1]);

		target[c] = (int16_t)((
			(_output_registers[8]&0xf) * _channel_ouput[0] +
			(_output_registers[9]&0xf) * _channel_ouput[1] +
			(_output_registers[10]&0xf) * _channel_ouput[2]
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
				break;

				case 12:
					_envelope_period = (_envelope_period & 0xff) | (uint16_t)(value << 8);
				break;
			}
			_output_registers[selected_register] = value;
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
