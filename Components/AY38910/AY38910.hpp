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

/*!
	Provides emulation of an AY-3-8910 / YM2149, which is a three-channel sound chip with a
	noise generator and a volume envelope generator, which also provides two bidirectional
	interface ports.
*/
class AY38910: public ::Outputs::Filter<AY38910> {
	public:
		/// Creates a new AY38910.
		AY38910();

		/// Sets the clock rate at which this AY38910 will be run.
		void set_clock_rate(double clock_rate);

		enum ControlLines {
			BC1		= (1 << 0),
			BC2		= (1 << 1),
			BCDIR	= (1 << 2)
		};

		/// Sets the value the AY would read from its data lines if it were not outputting.
		void set_data_input(uint8_t r);

		/// Gets the value that would appear on the data lines if only the AY is outputting.
		uint8_t get_data_output();

		/// Sets the current control line state, as a bit field.
		void set_control_lines(ControlLines control_lines);

		/*!
			Gets the value that would appear on the requested interface port if it were in output mode.
			@parameter port_b @c true to get the value for Port B, @c false to get the value for Port A.
		*/
		uint8_t get_port_output(bool port_b);

		// to satisfy ::Outputs::Speaker (included via ::Outputs::Filter; not for public consumption
		void get_samples(unsigned int number_of_samples, int16_t *target);

	private:
		int selected_register_;
		uint8_t registers_[16], output_registers_[16];

		int master_divider_;

		int tone_periods_[3];
		int tone_counters_[3];
		int tone_outputs_[3];

		int noise_period_;
		int noise_counter_;
		int noise_shift_register_;
		int noise_output_;

		int envelope_period_;
		int envelope_divider_;
		int envelope_position_;
		int envelope_shapes_[16][32];
		int envelope_overflow_masks_[16];

		int volumes_[16];

		enum ControlState {
			Inactive,
			LatchAddress,
			Read,
			Write
		} control_state_;

		void select_register(uint8_t r);
		void set_register_value(uint8_t value);
		uint8_t get_register_value();

		uint8_t data_input_, data_output_;

		int16_t output_volume_;
		inline void evaluate_output_volume();
};

};

#endif /* AY_3_8910_hpp */
