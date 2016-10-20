//
//  AY-3-8910.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 14/10/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#ifndef AY_3_8910_hpp
#define AY_3_8910_hpp

#include "../../Outputs/Speaker.hpp"

namespace GI {

class AY38910: public ::Outputs::Filter<AY38910> {
	public:
		AY38910();
		void set_clock_rate(double clock_rate);

		void get_samples(unsigned int number_of_samples, int16_t *target);
		void skip_samples(unsigned int number_of_samples);

		enum ControlLines {
			BC1		= (1 << 0),
			BC2		= (1 << 1),
			BCDIR	= (1 << 2)
		};
		void set_data_input(uint8_t r);
		uint8_t get_data_output();
		void set_control_lines(ControlLines control_lines);

		uint8_t get_port_output(bool port_b);

	private:
		int _selected_register;
		uint8_t _registers[16], _output_registers[16];

		int _tone_generator_controls[3];
		int _channel_dividers[3];
		int _channel_output[3];


		int _master_divider;

		int _noise_divider;
		int _noise_shift_register;
		int _noise_output;

		int _envelope_period;
		int _envelope_divider;

		int _envelope_position;
		int _envelope_shapes[16][32];
		int _envelope_overflow_masks[16];

		enum ControlState {
			Inactive,
			LatchAddress,
			Read,
			Write
		} _control_state;

		void select_register(uint8_t r);
		void set_register_value(uint8_t value);
		uint8_t get_register_value();

		uint8_t _data_input, _data_output;
};

};

#endif /* AY_3_8910_hpp */
