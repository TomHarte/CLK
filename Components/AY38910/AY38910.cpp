//
//  AY-3-8910.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 14/10/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#include "AY38910.hpp"

using namespace GI::AY38910;

AY38910::AY38910() :
		selected_register_(0),
		tone_counters_{0, 0, 0}, tone_periods_{0, 0, 0}, tone_outputs_{0, 0, 0},
		noise_shift_register_(0xffff), noise_period_(0), noise_counter_(0), noise_output_(0),
		envelope_divider_(0), envelope_period_(0), envelope_position_(0),
		master_divider_(0),
		output_registers_{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
		port_handler_(nullptr) {
	output_registers_[8] = output_registers_[9] = output_registers_[10] = 0;

	// set up envelope lookup tables
	for(int c = 0; c < 16; c++) {
		for(int p = 0; p < 32; p++) {
			switch(c) {
				case 0: case 1: case 2: case 3: case 9:
					envelope_shapes_[c][p] = (p < 16) ? (p^0xf) : 0;
					envelope_overflow_masks_[c] = 0x1f;
				break;
				case 4: case 5: case 6: case 7: case 15:
					envelope_shapes_[c][p] = (p < 16) ? p : 0;
					envelope_overflow_masks_[c] = 0x1f;
				break;

				case 8:
					envelope_shapes_[c][p] = (p & 0xf) ^ 0xf;
					envelope_overflow_masks_[c] = 0x00;
				break;
				case 12:
					envelope_shapes_[c][p] = (p & 0xf);
					envelope_overflow_masks_[c] = 0x00;
				break;

				case 10:
					envelope_shapes_[c][p] = (p & 0xf) ^ ((p < 16) ? 0xf : 0x0);
					envelope_overflow_masks_[c] = 0x00;
				break;
				case 14:
					envelope_shapes_[c][p] = (p & 0xf) ^ ((p < 16) ? 0x0 : 0xf);
					envelope_overflow_masks_[c] = 0x00;
				break;

				case 11:
					envelope_shapes_[c][p] = (p < 16) ? (p^0xf) : 0xf;
					envelope_overflow_masks_[c] = 0x1f;
				break;
				case 13:
					envelope_shapes_[c][p] = (p < 16) ? p : 0xf;
					envelope_overflow_masks_[c] = 0x1f;
				break;
			}
		}
	}

	// set up volume lookup table
	float max_volume = 8192;
	float root_two = sqrtf(2.0f);
	for(int v = 0; v < 16; v++) {
		volumes_[v] = (int)(max_volume / powf(root_two, (float)(v ^ 0xf)));
	}
	volumes_[0] = 0;
}

void AY38910::set_clock_rate(double clock_rate) {
	set_input_rate((float)clock_rate);
}

void AY38910::get_samples(unsigned int number_of_samples, int16_t *target) {
	unsigned int c = 0;
	while((master_divider_&7) && c < number_of_samples) {
		target[c] = output_volume_;
		master_divider_++;
		c++;
	}

	while(c < number_of_samples) {
#define step_channel(c) \
	if(tone_counters_[c]) tone_counters_[c]--;\
	else {\
		tone_outputs_[c] ^= 1;\
		tone_counters_[c] = tone_periods_[c];\
	}

		// update the tone channels
		step_channel(0);
		step_channel(1);
		step_channel(2);

#undef step_channel

		// ... the noise generator. This recomputes the new bit repeatedly but harmlessly, only shifting
		// it into the official 17 upon divider underflow.
		if(noise_counter_) noise_counter_--;
		else {
			noise_counter_ = noise_period_;
			noise_output_ ^= noise_shift_register_&1;
			noise_shift_register_ |= ((noise_shift_register_ ^ (noise_shift_register_ >> 3))&1) << 17;
			noise_shift_register_ >>= 1;
		}

		// ... and the envelope generator. Table based for pattern lookup, with a 'refill' step — a way of
		// implementing non-repeating patterns by locking them to table position 0x1f.
		if(envelope_divider_) envelope_divider_--;
		else {
			envelope_divider_ = envelope_period_;
			envelope_position_ ++;
			if(envelope_position_ == 32) envelope_position_ = envelope_overflow_masks_[output_registers_[13]];
		}

		evaluate_output_volume();

		for(int ic = 0; ic < 8 && c < number_of_samples; ic++) {
			target[c] = output_volume_;
			c++;
			master_divider_++;
		}
	}

	master_divider_ &= 7;
}

void AY38910::evaluate_output_volume() {
	int envelope_volume = envelope_shapes_[output_registers_[13]][envelope_position_];

	// The output level for a channel is:
	//	1 if neither tone nor noise is enabled;
	//	0 if either tone or noise is enabled and its value is low.
	// The tone/noise enable bits use inverse logic — 0 = on, 1 = off — permitting the OR logic below.
#define tone_level(c, tone_bit)		(tone_outputs_[c] | (output_registers_[7] >> tone_bit))
#define noise_level(c, noise_bit)	(noise_output_ | (output_registers_[7] >> noise_bit))

#define level(c, tone_bit, noise_bit)	tone_level(c, tone_bit) & noise_level(c, noise_bit) & 1
	const int channel_levels[3] = {
		level(0, 0, 3),
		level(1, 1, 4),
		level(2, 2, 5),
	};
#undef level

		// Channel volume is a simple selection: if the bit at 0x10 is set, use the envelope volume; otherwise use the lower four bits
#define channel_volume(c)	\
	((output_registers_[c] >> 4)&1) * envelope_volume + (((output_registers_[c] >> 4)&1)^1) * (output_registers_[c]&0xf)

	const int volumes[3] = {
		channel_volume(8),
		channel_volume(9),
		channel_volume(10)
	};
#undef channel_volume

	// Mix additively.
	output_volume_ = (int16_t)(
		volumes_[volumes[0]] * channel_levels[0] +
		volumes_[volumes[1]] * channel_levels[1] +
		volumes_[volumes[2]] * channel_levels[2]
	);
}

#pragma mark - Register manipulation

void AY38910::select_register(uint8_t r) {
	selected_register_ = r;
}

void AY38910::set_register_value(uint8_t value) {
	if(selected_register_ > 15) return;
	registers_[selected_register_] = value;
	if(selected_register_ < 14) {
		int selected_register = selected_register_;
		enqueue([=] () {
			uint8_t masked_value = value;
			switch(selected_register) {
				case 0: case 2: case 4:
				case 1: case 3: case 5: {
					int channel = selected_register >> 1;

					if(selected_register & 1)
						tone_periods_[channel] = (tone_periods_[channel] & 0xff) | (uint16_t)((value&0xf) << 8);
					else
						tone_periods_[channel] = (tone_periods_[channel] & ~0xff) | value;
				}
				break;

				case 6:
					noise_period_ = value & 0x1f;
				break;

				case 11:
					envelope_period_ = (envelope_period_ & ~0xff) | value;
				break;

				case 12:
					envelope_period_ = (envelope_period_ & 0xff) | (int)(value << 8);
				break;

				case 13:
					masked_value &= 0xf;
					envelope_position_ = 0;
				break;
			}
			output_registers_[selected_register] = masked_value;
			evaluate_output_volume();
		});
	} else {
		if(port_handler_) port_handler_->set_port_output(selected_register_ == 15, value);
	}
}

uint8_t AY38910::get_register_value() {
	// This table ensures that bits that aren't defined within the AY are returned as 1s
	// when read. I can't find documentation on this and don't have a machine to test, so
	// this is provisionally a guess. TODO: investigate.
	const uint8_t register_masks[16] = {
		0x00, 0xf0, 0x00, 0xf0, 0x00, 0xf0, 0xe0, 0x00,
		0xe0, 0xe0, 0xe0, 0x00, 0x00, 0xf0, 0x00, 0x00
	};

	if(selected_register_ > 15) return 0xff;
	switch(selected_register_) {
		default:	return registers_[selected_register_] & ~register_masks[selected_register_];
		case 14:	return (registers_[0x7] & 0x40) ? registers_[14] : port_inputs_[0];
		case 15:	return (registers_[0x7] & 0x80) ? registers_[15] : port_inputs_[1];
	}
}

#pragma mark - Port handling

uint8_t AY38910::get_port_output(bool port_b) {
	return registers_[port_b ? 15 : 14];
}

#pragma mark - Bus handling

void AY38910::set_port_handler(PortHandler *handler) {
	port_handler_ = handler;
}

void AY38910::set_data_input(uint8_t r) {
	data_input_ = r;
	update_bus();
}

uint8_t AY38910::get_data_output() {
	if(control_state_ == Read && selected_register_ >= 14) {
		if(port_handler_) {
			return port_handler_->get_port_input(selected_register_ == 15);
		} else {
			return 0xff;
		}
	}
	return data_output_;
}

void AY38910::set_control_lines(ControlLines control_lines) {
	switch((int)control_lines) {
		default:					control_state_ = Inactive;		break;

		case (int)(BDIR | BC2 | BC1):
		case BDIR:
		case BC1:					control_state_ = LatchAddress;	break;

		case (int)(BC2 | BC1):		control_state_ = Read;			break;
		case (int)(BDIR | BC2):		control_state_ = Write;			break;
	}

	update_bus();
}

void AY38910::update_bus() {
	switch(control_state_) {
		default: break;
		case LatchAddress:	select_register(data_input_);			break;
		case Write:			set_register_value(data_input_);		break;
		case Read:			data_output_ = get_register_value();	break;
	}
}
