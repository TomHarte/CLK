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
#include <stdint.h>

namespace Storage {

class UEF : public Tape {
	public:
		UEF(const char *file_name);
		~UEF();

		Pulse get_next_pulse();
		void reset();

		enum {
			ErrorNotUEF
		};

	private:
		gzFile _file;
		unsigned int _time_base;
		z_off_t _start_of_next_chunk;

		uint16_t _chunk_id;
		uint32_t _chunk_length;

		union {
			struct {
				uint8_t current_byte;
				uint32_t position;
			} _implicit_data_chunk;

			struct {
				uint8_t current_byte;
				uint32_t position;
			} _explicit_data_chunk;
		};

		uint8_t _current_byte;
		uint32_t _chunk_position;

		bool _current_bit;
		uint32_t _bit_position;

		Time _chunk_duration;

		bool _first_is_pulse;
		bool _last_is_pulse;

		void find_next_tape_chunk();
		bool get_next_bit();
		bool chunk_is_finished();
};

}

#endif /* TapeUEF_hpp */
