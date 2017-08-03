//
//  TZX.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 16/07/2017.
//  Copyright © 2017 Thomas Harte. All rights reserved.
//

#ifndef TZX_hpp
#define TZX_hpp

#include "../PulseQueuedTape.hpp"
#include "../../FileHolder.hpp"

namespace Storage {
namespace Tape {

/*!
	Provides a @c Tape containing a CSW tape image, which is a compressed 1-bit sampling.
*/
class TZX: public PulseQueuedTape, public Storage::FileHolder {
	public:
		/*!
			Constructs a @c TZX containing content from the file with name @c file_name.

			@throws ErrorNotTZX if this file could not be opened and recognised as a valid TZX file.
		*/
		TZX(const char *file_name);

		enum {
			ErrorNotTZX
		};

	private:
		void virtual_reset();
		void get_next_pulses();

		bool current_level_;

		void get_standard_speed_data_block();
		void get_turbo_speed_data_block();
		void get_pure_tone_data_block();
		void get_pulse_sequence();
		void get_pure_data_block();
		void get_generalised_data_block();
		void get_pause();

		void get_hardware_type();

		void ignore_group_start();
		void ignore_group_end();
		void ignore_jump_to_block();
		void ignore_loop_start();
		void ignore_loop_end();
		void ignore_call_sequence();
		void ignore_return_from_sequence();
		void ignore_select_block();
		void ignore_text_description();
		void ignore_message_block();

		struct Data {
			unsigned int length_of_zero_bit_pulse;
			unsigned int length_of_one_bit_pulse;
			unsigned int number_of_bits_in_final_byte;
			unsigned int pause_after_block;
			long data_length;
		};

		struct DataBlock {
			unsigned int length_of_pilot_pulse;
			unsigned int length_of_sync_first_pulse;
			unsigned int length_of_sync_second_pulse;
			unsigned int length_of_pilot_tone;
			Data data;
		};

		void get_generalised_segment(uint32_t output_symbols, uint8_t max_pulses_per_symbol, uint8_t number_of_symbols, bool is_data);
		void get_data_block(const DataBlock &);
		void get_data(const Data &);

		void post_pulse(unsigned int length);
		void post_gap(unsigned int milliseconds);

		void post_pulse(const Storage::Time &time);
};

}
}
#endif /* TZX_hpp */
