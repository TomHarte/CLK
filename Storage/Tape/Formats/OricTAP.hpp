//
//  OricTAP.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 10/10/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#ifndef OricTAP_hpp
#define OricTAP_hpp

#include "../Tape.hpp"
#include <stdint.h>

namespace Storage {
namespace Tape {

/*!
	Provides a @c Tape containing an Oric-format tape image, which is a byte stream capture.
*/
class OricTAP: public Tape {
	public:
		/*!
			Constructs an @c OricTAP containing content from the file with name @c file_name.

			@throws ErrorNotOricTAP if this file could not be opened and recognised as a valid Oric-format TAP.
		*/
		OricTAP(const char *file_name);
		~OricTAP();

		enum {
			ErrorNotOricTAP
		};

		// implemented to satisfy @c Tape
		bool is_at_end();

	private:
		void virtual_reset();
		Pulse virtual_get_next_pulse();

		FILE *_file;
		size_t _file_length;

		uint16_t _current_value;
		int _bit_count;
		int _pulse_counter;
		int _phase_counter;

		enum Phase {
			LeadIn, Header, Data, End
		} _phase, _next_phase;
		uint16_t _data_end_address, _data_start_address;
};

}
}

#endif /* OricTAP_hpp */
