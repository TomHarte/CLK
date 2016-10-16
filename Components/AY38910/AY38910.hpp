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

		void select_register(uint8_t r);
		void set_register_value(uint8_t value);
		uint8_t get_register_value();

		uint8_t get_port_output(bool port_b);

	private:
		int _selected_register;
		uint8_t _registers[16], _output_registers[16];

		uint16_t _tone_generator_controls[3];
		uint16_t _envelope_period;

		int _master_divider;
		int _channel_dividers[3];
		int _channel_ouput[3];
};

};

#endif /* AY_3_8910_hpp */
