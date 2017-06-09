//
//  ZX80O.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 04/06/2017.
//  Copyright © 2017 Thomas Harte. All rights reserved.
//

#ifndef ZX80O_hpp
#define ZX80O_hpp

#include "../Tape.hpp"
#include "../../FileHolder.hpp"
#include <cstdint>

namespace Storage {
namespace Tape {

/*!
	Provides a @c Tape containing a ZX80-format .O tape image, which is a byte stream capture.
*/
class ZX80O: public Tape, public Storage::FileHolder {
	public:
		/*!
			Constructs an @c ZX80O containing content from the file with name @c file_name.

			@throws ErrorNotZX80O if this file could not be opened and recognised as a valid ZX80-format .O.
		*/
		ZX80O(const char *file_name);

		enum {
			ErrorNotZX80O
		};

		// implemented to satisfy @c Tape
		bool is_at_end();

	private:
		void virtual_reset();
		Pulse virtual_get_next_pulse();
		bool has_finished_data();

		uint8_t byte_;
		int bit_pointer_;
		int wave_pointer_;
		bool is_past_silence_, has_ended_final_byte_;
		bool is_high_;

		uint16_t end_of_file_;
};

}
}

#endif /* ZX80O_hpp */
