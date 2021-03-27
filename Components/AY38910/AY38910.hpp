//
//  AY-3-8910.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 14/10/2016.
//  Copyright 2016 Thomas Harte. All rights reserved.
//

#ifndef AY_3_8910_hpp
#define AY_3_8910_hpp

#include "../../Outputs/Speaker/Implementation/SampleSource.hpp"
#include "../../Concurrency/AsyncTaskQueue.hpp"

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
		virtual uint8_t get_port_input([[maybe_unused]] bool port_b) {
			return 0xff;
		}

		/*!
			Sets the current output on an AY port.

			@param port_b @c true if the output being posted is Port B. @c false if it is Port A.
			@param value the value now being output.
		*/
		virtual void set_port_output([[maybe_unused]] bool port_b, [[maybe_unused]] uint8_t value) {}
};

/*!
	Names the control lines used as input to the AY, which uses CP1600 bus semantics.
*/
enum ControlLines {
	BC1		= (1 << 0),
	BC2		= (1 << 1),
	BDIR	= (1 << 2)
};

enum class Personality {
	/// Provides 16 volume levels to envelopes.
	AY38910,
	/// Provides 32 volume levels to envelopes.
	YM2149F
};

/*!
	Provides emulation of an AY-3-8910 / YM2149, which is a three-channel sound chip with a
	noise generator and a volume envelope generator, which also provides two bidirectional
	interface ports.

	This AY has an attached mono or stereo mixer.
*/
template <bool is_stereo> class AY38910: public ::Outputs::Speaker::SampleSource {
	public:
		/// Creates a new AY38910.
		AY38910(Personality, Concurrency::DeferringAsyncTaskQueue &);

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

		/*!
			Enables or disables stereo output; if stereo output is enabled then also sets the weight of each of the AY's
			channels in each of the output channels.

			If a_left_ = b_left = c_left = a_right = b_right = c_right = 1.0 then you'll get output that's effectively mono.

			a_left = 0.0, a_right = 1.0 will make A full volume on the right output, and silent on the left.

			a_left = 0.5, a_right = 0.5 will make A half volume on both outputs.
		*/
		void set_output_mixing(float a_left, float b_left, float c_left, float a_right = 1.0, float b_right = 1.0, float c_right = 1.0);

		// to satisfy ::Outputs::Speaker (included via ::Outputs::Filter.
		void get_samples(std::size_t number_of_samples, int16_t *target);
		bool is_zero_level() const;
		void set_sample_volume_range(std::int16_t range);
		static constexpr bool get_is_stereo() { return is_stereo; }

	private:
		Concurrency::DeferringAsyncTaskQueue &task_queue_;

		int selected_register_ = 0;
		uint8_t registers_[16] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
		uint8_t output_registers_[16] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

		int master_divider_ = 0;

		int tone_periods_[3] = {0, 0, 0};
		int tone_counters_[3] = {0, 0, 0};
		int tone_outputs_[3] = {0, 0, 0};

		int noise_period_ = 0;
		int noise_counter_ = 0;
		int noise_shift_register_ = 0xffff;
		int noise_output_ = 0;

		int envelope_period_ = 0;
		int envelope_divider_ = 0;
		int envelope_position_ = 0, envelope_position_mask_ = 0;
		int envelope_shapes_[16][64];
		int envelope_overflow_masks_[16];

		int volumes_[32];

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

		uint32_t output_volume_;

		void update_bus();
		PortHandler *port_handler_ = nullptr;
		void set_port_output(bool port_b);

		void evaluate_output_volume();

		// Output mixing control.
		uint8_t a_left_ = 255, a_right_ = 255;
		uint8_t b_left_ = 255, b_right_ = 255;
		uint8_t c_left_ = 255, c_right_ = 255;
};

/*!
	Provides helper code, to provide something closer to the interface exposed by many
	AY-deploying machines of the era.
*/
struct Utility {
	template <typename AY> static void write(AY &ay, bool is_data_write, uint8_t data) {
		ay.set_control_lines(GI::AY38910::ControlLines(GI::AY38910::BDIR | GI::AY38910::BC2 | (is_data_write ? 0 : GI::AY38910::BC1)));
		ay.set_data_input(data);
		ay.set_control_lines(GI::AY38910::ControlLines(0));
	}

	template <typename AY> static void select_register(AY &ay, uint8_t reg) {
		write(ay, false, reg);
	}

	template <typename AY> static void write_data(AY &ay, uint8_t reg) {
		write(ay, true, reg);
	}

	template <typename AY> static uint8_t read(AY &ay) {
		ay.set_control_lines(GI::AY38910::ControlLines(GI::AY38910::BC2 | GI::AY38910::BC1));
		const uint8_t result = ay.get_data_output();
		ay.set_control_lines(GI::AY38910::ControlLines(0));
		return result;
	}

};

}
}

#endif /* AY_3_8910_hpp */
