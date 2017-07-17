//
//  TZX.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 16/07/2017.
//  Copyright © 2017 Thomas Harte. All rights reserved.
//

#ifndef TZX_hpp
#define TZX_hpp

#include "../Tape.hpp"
#include "../../FileHolder.hpp"

namespace Storage {
namespace Tape {

/*!
	Provides a @c Tape containing a CSW tape image, which is a compressed 1-bit sampling.
*/
class TZX: public Tape, public Storage::FileHolder {
	public:
		/*!
			Constructs a @c TZX containing content from the file with name @c file_name.

			@throws ErrorNotTZX if this file could not be opened and recognised as a valid TZX file.
		*/
		TZX(const char *file_name);

		enum {
			ErrorNotTZX
		};

		// implemented to satisfy @c Tape
		bool is_at_end();

	private:
		Pulse virtual_get_next_pulse();
		void virtual_reset();

//		std::vector<Pulse> queued_pulses_;
		size_t pulse_pointer_;
		bool is_at_end_;

		bool is_high_;
		void parse_next_chunk();
};

}
}
#endif /* TZX_hpp */
