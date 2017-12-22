//
//  CAS.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 25/11/2017.
//  Copyright © 2017 Thomas Harte. All rights reserved.
//

#ifndef CAS_hpp
#define CAS_hpp

#include "../Tape.hpp"
#include "../../FileHolder.hpp"

namespace Storage {
namespace Tape {

/*!
	Provides a @c Tape containing a CAS tape image, which is an MSX byte stream.
*/
class CAS: public Tape {
	public:
		/*!
			Constructs a @c CAS containing content from the file with name @c file_name.

			@throws ErrorNotCAS if this file could not be opened and recognised as a valid CAS file.
		*/
		CAS(const char *file_name);

		enum {
			ErrorNotCAS
		};

		// implemented to satisfy @c Tape
		bool is_at_end();

	private:
		Storage::FileHolder file_;

		void virtual_reset();
		Pulse virtual_get_next_pulse();

		uint8_t input_[8] = {0, 0, 0, 0, 0, 0, 0, 0};

		enum class Phase {
			Header,
			Bytes,
			Gap
		} phase_ = Phase::Header;
		int distance_into_phase_ = 0;
};

}
}

#endif /* CAS_hpp */
