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
namespace AY38910 {

/*!
	A port handler provides all input for an AY's two 8-bit ports, and may optionally receive
	active notification of changes in output.

	Machines with an AY without ports or with nothing wired to them need not supply a port handler.
	Machines that use the AY ports as output but for which polling for changes is acceptable can
	instead use AY38910.get_port_output.
*/
class PortHandler {
	public:
		/*!
			Requests the current input on an AY port.

			@param port_b @c true if the input being queried is Port B. @c false if it is Port A.
		*/
		virtual uint8_t get_port_input(bool port_b) {
			return 0xff;
		}

		/*!
			Requests the current input on an AY port.

			@param port_b @c true if the input being queried is Port B. @c false if it is Port A.
		*/
		virtual void set_port_output(bool port_b, uint8_t value) {}
};

/*!
	Names the control lines used as input to the AY, which uses CP1600 bus semantics.
*/
enum ControlLines {
	BC1		= (1 << 0),
	BC2		= (1 << 1),
	BDIR	= (1 << 2)
};

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

		/*!
			Sets the port handler, which will receive a call every time the AY either wants to sample
			input or else declare new output. As a convenience, current port output can be obtained
			without installing a port handler via get_port_output.
		*/
		void set_port_handler(PortHandler *);

		// to satisfy ::Outputs::Speaker (included via ::Outputs::Filter; not for public consumption
		void get_samples(unsigned int number_of_samples, int16_t *target);

	private:
		int selected_register_;
		uint8_t registers_[16], output_registers_[16];
		uint8_t port_inputs_[2];

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

		inline void update_bus();
		PortHandler *port_handler_;
};

}
}

#endif /* AY_3_8910_hpp */
