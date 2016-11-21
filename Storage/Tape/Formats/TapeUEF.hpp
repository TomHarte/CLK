//
//  TapeUEF.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 18/01/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#ifndef TapeUEF_hpp
#define TapeUEF_hpp

#include "../Tape.hpp"
#include <zlib.h>
#include <cstdint>
#include <vector>

namespace Storage {
namespace Tape {

/*!
	Provides a @c Tape containing a UEF tape image, a slightly-convoluted description of pulses.
*/
class UEF : public Tape {
	public:
		/*!
			Constructs a @c UEF containing content from the file with name @c file_name.

			@throws ErrorNotUEF if this file could not be opened and recognised as a valid UEF.
		*/
		UEF(const char *file_name);
		~UEF();

		enum {
			ErrorNotUEF
		};

		// implemented to satisfy @c Tape
		bool is_at_end();

	private:
		void virtual_reset();
		Pulse virtual_get_next_pulse();

		gzFile file_;
		unsigned int time_base_;
		bool is_at_end_;
		bool is_300_baud_;

		std::vector<Pulse> queued_pulses_;
		size_t pulse_pointer_;

		void parse_next_tape_chunk();

		void queue_implicit_bit_pattern(uint32_t length);
		void queue_explicit_bit_pattern(uint32_t length);

		void queue_integer_gap();
		void queue_floating_point_gap();

		void queue_carrier_tone();
		void queue_carrier_tone_with_dummy();

		void queue_security_cycles();
		void queue_defined_data(uint32_t length);

		void queue_bit(int bit);
		void queue_implicit_byte(uint8_t byte);
};

}
}

#endif /* TapeUEF_hpp */
