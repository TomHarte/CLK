//
//  OricTAP.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 10/10/2016.
//  Copyright 2016 Thomas Harte. All rights reserved.
//

#ifndef OricTAP_hpp
#define OricTAP_hpp

#include "../Tape.hpp"
#include "../../FileHolder.hpp"

#include <cstdint>
#include <string>

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
		OricTAP(const std::string &file_name);

		enum {
			ErrorNotOricTAP
		};

		// implemented to satisfy @c Tape
		bool is_at_end();

	private:
		Storage::FileHolder file_;
		void virtual_reset();
		Pulse virtual_get_next_pulse();

		// byte serialisation and output
		uint16_t current_value_;
		int bit_count_;
		int pulse_counter_;

		enum Phase {
			LeadIn, Header, Data, Gap, End
		} phase_, next_phase_;
		int phase_counter_;
		uint16_t data_end_address_, data_start_address_;
};

}
}

#endif /* OricTAP_hpp */
