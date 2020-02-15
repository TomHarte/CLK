//
//  AY-3-8910.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 14/10/2016.
//  Copyright 2016 Thomas Harte. All rights reserved.
//

#include "AY38910.hpp"

#include <cmath>

using namespace GI::AY38910;

AY38910::AY38910(Personality personality, Concurrency::DeferringAsyncTaskQueue &task_queue) : task_queue_(task_queue) {
	// Don't use the low bit of the envelope position if this is an AY.
	envelope_position_mask_ |= personality == Personality::AY38910;

	// Set up envelope lookup tables.
	for(int c = 0; c < 16; c++) {
		for(int p = 0; p < 64; p++) {
			switch(c) {
				case 0: case 1: case 2: case 3: case 9:
					/* Envelope: \____ */
					envelope_shapes_[c][p] = (p < 32) ? (p^0x1f) : 0;
					envelope_overflow_masks_[c] = 0x3f;
				break;
				case 4: case 5: case 6: case 7: case 15:
					/* Envelope: /____ */
					envelope_shapes_[c][p] = (p < 32) ? p : 0;
					envelope_overflow_masks_[c] = 0x3f;
				break;

				case 8:
					/* Envelope: \\\\\\\\ */
					envelope_shapes_[c][p] = (p & 0x1f) ^ 0x1f;
					envelope_overflow_masks_[c] = 0x00;
				break;
				case 12:
					/* Envelope: //////// */
					envelope_shapes_[c][p] = (p & 0x1f);
					envelope_overflow_masks_[c] = 0x00;
				break;

				case 10:
					/* Envelope: \/\/\/\/ */
					envelope_shapes_[c][p] = (p & 0x1f) ^ ((p < 32) ? 0x1f : 0x0);
					envelope_overflow_masks_[c] = 0x00;
				break;
				case 14:
					/* Envelope: /\/\/\/\ */
					envelope_shapes_[c][p] = (p & 0x1f) ^ ((p < 32) ? 0x0 : 0x1f);
					envelope_overflow_masks_[c] = 0x00;
				break;

				case 11:
					/* Envelope: \------	(if - is high) */
					envelope_shapes_[c][p] = (p < 32) ? (p^0x1f) : 0x1f;
					envelope_overflow_masks_[c] = 0x3f;
				break;
				case 13:
					/* Envelope: /------- */
					envelope_shapes_[c][p] = (p < 32) ? p : 0x1f;
					envelope_overflow_masks_[c] = 0x3f;
				break;
			}
		}
	}

	set_sample_volume_range(0);
}

void AY38910::set_sample_volume_range(std::int16_t range) {
	// Set up volume lookup table; the function below is based on a combination of the graph
	// from the YM's datasheet, showing a clear power curve, and fitting that to observed
	// values reported elsewhere.
	const float max_volume = float(range) / 3.0f;	// As there are three channels.
	constexpr float root_two = 1.414213562373095f;
	for(int v = 0; v < 32; v++) {
		volumes_[v] = int(max_volume / powf(root_two, float(v ^ 0x1f) / 3.18f));
	}

	// Tie level 0 to silence.
	for(int v = 31; v >= 0; --v) {
		volumes_[v] -= volumes_[0];
	}

	if(is_stereo_) {
		evaluate_output_volume<true>();
	} else {
		evaluate_output_volume<false>();
	}
}

void AY38910::set_output_mixing(bool is_stereo, float a_left, float b_left, float c_left, float a_right, float b_right, float c_right) {
	is_stereo_ = is_stereo;
	a_left_ = uint8_t(a_left * 255.0f);
	b_left_ = uint8_t(b_left * 255.0f);
	c_left_ = uint8_t(c_left * 255.0f);
	a_right_ = uint8_t(a_right * 255.0f);
	b_right_ = uint8_t(b_right * 255.0f);
	c_right_ = uint8_t(c_right * 255.0f);
}

void AY38910::get_samples(std::size_t number_of_samples, int16_t *target) {
	if(is_stereo_) {
		get_samples<true>(number_of_samples, target);
	} else {
		get_samples<false>(number_of_samples, target);
	}
}

template <bool is_stereo> void AY38910::get_samples(std::size_t number_of_samples, int16_t *target) {
	// Note on structure below: the real AY has a built-in divider of 8
	// prior to applying its tone and noise dividers. But the YM fills the
	// same total periods for noise and tone with double-precision envelopes.
	// Therefore this class implements a divider of 4 and doubles the tone
	// and noise periods. The envelope ticks along at the divide-by-four rate,
	// but if this is an AY rather than a YM then its lowest bit is forced to 1,
	// matching the YM datasheet's depiction of envelope level 31 as equal to
	// programmatic volume 15, envelope level 29 as equal to programmatic 14, etc.

	std::size_t c = 0;
	while((master_divider_&3) && c < number_of_samples) {
		if constexpr (is_stereo) {
			reinterpret_cast<uint32_t *>(target)[c] = output_volume_;
		} else {
			target[c] = output_volume_;
		}
		master_divider_++;
		c++;
	}

	while(c < number_of_samples) {
#define step_channel(c) \
	if(tone_counters_[c]) tone_counters_[c]--;\
	else {\
		tone_outputs_[c] ^= 1;\
		tone_counters_[c] = tone_periods_[c] << 1;\
	}

		// Update the tone channels.
		step_channel(0);
		step_channel(1);
		step_channel(2);

#undef step_channel

		// Update the noise generator. This recomputes the new bit repeatedly but harmlessly, only shifting
		// it into the official 17 upon divider underflow.
		if(noise_counter_) noise_counter_--;
		else {
			noise_counter_ = noise_period_ << 1;	// To cover the double resolution of envelopes.
			noise_output_ ^= noise_shift_register_&1;
			noise_shift_register_ |= ((noise_shift_register_ ^ (noise_shift_register_ >> 3))&1) << 17;
			noise_shift_register_ >>= 1;
		}

		// Update the envelope generator. Table based for pattern lookup, with a 'refill' step: a way of
		// implementing non-repeating patterns by locking them to the final table position.
		if(envelope_divider_) envelope_divider_--;
		else {
			envelope_divider_ = envelope_period_;
			envelope_position_ ++;
			if(envelope_position_ == 64) envelope_position_ = envelope_overflow_masks_[output_registers_[13]];
		}

		evaluate_output_volume<is_stereo>();

		for(int ic = 0; ic < 4 && c < number_of_samples; ic++) {
			if constexpr (is_stereo) {
				reinterpret_cast<uint32_t *>(target)[c] = output_volume_;
			} else {
				target[c] = output_volume_;
			}
			c++;
			master_divider_++;
		}
	}

	master_divider_ &= 3;
}

template <bool is_stereo> void AY38910::evaluate_output_volume() {
	int envelope_volume = envelope_shapes_[output_registers_[13]][envelope_position_ | envelope_position_mask_];

	// The output level for a channel is:
	//	1 if neither tone nor noise is enabled;
	//	0 if either tone or noise is enabled and its value is low.
	// The tone/noise enable bits use inverse logic; 0 = on, 1 = off; permitting the OR logic below.
#define tone_level(c, tone_bit)		(tone_outputs_[c] | (output_registers_[7] >> tone_bit))
#define noise_level(c, noise_bit)	(noise_output_ | (output_registers_[7] >> noise_bit))

#define level(c, tone_bit, noise_bit)	tone_level(c, tone_bit) & noise_level(c, noise_bit) & 1
	const int channel_levels[3] = {
		level(0, 0, 3),
		level(1, 1, 4),
		level(2, 2, 5),
	};
#undef level

	// This remapping table seeks to map 'channel volumes', i.e. the levels produced from the
	// 16-step progammatic volumes set per channel to 'envelope volumes', i.e. the 32-step
	// volumes that are produced by the envelope generators (on a YM at least). My reading of
	// the data sheet is that '0' is still off, but 15 should be as loud as peak envelope. So
	// I've thrown in the discontinuity at the low end, where it'll be very quiet.
	const int channel_volumes[] = {
		0, 3, 5, 7, 9, 11, 13, 15, 17, 19, 21, 23, 25, 27, 29, 31
	};
	static_assert(sizeof(channel_volumes) == 16*sizeof(int));

		// Channel volume is a simple selection: if the bit at 0x10 is set, use the envelope volume; otherwise use the lower four bits,
		// mapped to the range 1–31 in case this is a YM.
#define channel_volume(c)	\
	((output_registers_[c] >> 4)&1) * envelope_volume + (((output_registers_[c] >> 4)&1)^1) * channel_volumes[output_registers_[c]&0xf]

	const int volumes[3] = {
		channel_volume(8),
		channel_volume(9),
		channel_volume(10)
	};
#undef channel_volume

	// Mix additively, weighting if in stereo.
	if constexpr (is_stereo) {
		int16_t *const volumes = reinterpret_cast<int16_t *>(&output_volume_);
		volumes[0] = int16_t((
			volumes_[volumes[0]] * channel_levels[0] * a_left_ +
			volumes_[volumes[1]] * channel_levels[1] * b_left_ +
			volumes_[volumes[2]] * channel_levels[2] * c_left_
		) >> 8);
		volumes[1] = int16_t((
			volumes_[volumes[0]] * channel_levels[0] * a_right_ +
			volumes_[volumes[1]] * channel_levels[1] * b_right_ +
			volumes_[volumes[2]] * channel_levels[2] * c_right_
		) >> 8);
	} else {
		output_volume_ = int16_t(
			volumes_[volumes[0]] * channel_levels[0] +
			volumes_[volumes[1]] * channel_levels[1] +
			volumes_[volumes[2]] * channel_levels[2]
		);
	}
}

bool AY38910::is_zero_level() {
	// Confirm that the AY is trivially at the zero level if all three volume controls are set to fixed zero.
	return output_registers_[0x8] == 0 && output_registers_[0x9] == 0 && output_registers_[0xa] == 0;
}

// MARK: - Register manipulation

void AY38910::select_register(uint8_t r) {
	selected_register_ = r;
}

void AY38910::set_register_value(uint8_t value) {
	// There are only 16 registers.
	if(selected_register_ > 15) return;

	// If this is a register that affects audio output, enqueue a mutation onto the
	// audio generation thread.
	if(selected_register_ < 14) {
		task_queue_.defer([this, selected_register = selected_register_, value] () {
			// Perform any register-specific mutation to output generation.
			uint8_t masked_value = value;
			switch(selected_register) {
				case 0: case 2: case 4:
				case 1: case 3: case 5: {
					int channel = selected_register >> 1;

					if(selected_register & 1)
						tone_periods_[channel] = (tone_periods_[channel] & 0xff) | uint16_t((value&0xf) << 8);
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
					envelope_period_ = (envelope_period_ & 0xff) | int(value << 8);
				break;

				case 13:
					masked_value &= 0xf;
					envelope_position_ = 0;
				break;
			}

			// Store a copy of the current register within the storage used by the audio generation
			// thread, and apply any changes to output volume.
			output_registers_[selected_register] = masked_value;
			if(is_stereo_) {
				evaluate_output_volume<true>();
			} else {
				evaluate_output_volume<false>();
			}
		});
	}

	// Decide which outputs are going to need updating (if any).
	bool update_port_a = false;
	bool update_port_b = true;
	if(port_handler_) {
		if(selected_register_ == 7) {
			const uint8_t io_change = registers_[7] ^ value;
			update_port_b = !!(io_change&0x80);
			update_port_a = !!(io_change&0x40);
		} else {
			update_port_b = selected_register_ == 15;
			update_port_a = selected_register_ != 15;
		}
	}

	// Keep a copy of the new value that is usable from the emulation thread.
	registers_[selected_register_] = value;

	// Update ports as required.
	if(update_port_b) set_port_output(true);
	if(update_port_a) set_port_output(false);
}

uint8_t AY38910::get_register_value() {
	// This table ensures that bits that aren't defined within the AY are returned as 0s
	// when read, conforming to CPC-sourced unit tests.
	const uint8_t register_masks[16] = {
		0xff, 0x0f, 0xff, 0x0f, 0xff, 0x0f, 0x1f, 0xff,
		0x1f, 0x1f, 0x1f, 0xff, 0xff, 0x0f, 0xff, 0xff
	};

	if(selected_register_ > 15) return 0xff;
	return registers_[selected_register_] & register_masks[selected_register_];
}

// MARK: - Port querying

uint8_t AY38910::get_port_output(bool port_b) {
	return registers_[port_b ? 15 : 14];
}

// MARK: - Bus handling

void AY38910::set_port_handler(PortHandler *handler) {
	port_handler_ = handler;
	set_port_output(true);
	set_port_output(false);
}

void AY38910::set_data_input(uint8_t r) {
	data_input_ = r;
	update_bus();
}

void AY38910::set_port_output(bool port_b) {
	// Per the data sheet: "each [IO] pin is provided with an on-chip pull-up resistor,
	// so that when in the "input" mode, all pins will read normally high". Therefore,
	// report programmer selection of input mode as creating an output of 0xff.
	if(port_handler_) {
		const bool is_output = !!(registers_[7] & (port_b ? 0x80 : 0x40));
		port_handler_->set_port_output(port_b, is_output ? registers_[port_b ? 15 : 14] : 0xff);
	}
}

uint8_t AY38910::get_data_output() {
	if(control_state_ == Read && selected_register_ >= 14 && selected_register_ < 16) {
		// Per http://cpctech.cpc-live.com/docs/psgnotes.htm if a port is defined as output then the
		// value returned to the CPU when reading it is the and of the output value and any input.
		// If it's defined as input then you just get the input.
		const uint8_t mask = port_handler_ ? port_handler_->get_port_input(selected_register_ == 15) : 0xff;

		switch(selected_register_) {
			default:	break;
			case 14:	return mask & ((registers_[0x7] & 0x40) ? registers_[14] : 0xff);
			case 15:	return mask & ((registers_[0x7] & 0x80) ? registers_[15] : 0xff);
		}
	}
	return data_output_;
}

void AY38910::set_control_lines(ControlLines control_lines) {
	switch(int(control_lines)) {
		default:					control_state_ = Inactive;		break;

		case int(BDIR | BC2 | BC1):
		case BDIR:
		case BC1:					control_state_ = LatchAddress;	break;

		case int(BC2 | BC1):		control_state_ = Read;			break;
		case int(BDIR | BC2):		control_state_ = Write;			break;
	}

	update_bus();
}

void AY38910::update_bus() {
	// Assume no output, unless this turns out to be a read.
	data_output_ = 0xff;
	switch(control_state_) {
		default:			break;
		case LatchAddress:	select_register(data_input_);			break;
		case Write:			set_register_value(data_input_);		break;
		case Read:			data_output_ = get_register_value();	break;
	}
}
