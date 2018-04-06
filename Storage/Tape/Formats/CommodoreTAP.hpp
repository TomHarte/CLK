//
//  CommodoreTAP.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 25/06/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#ifndef CommodoreTAP_hpp
#define CommodoreTAP_hpp

#include "../Tape.hpp"
#include "../../FileHolder.hpp"

#include <cstdint>
#include <string>

namespace Storage {
namespace Tape {

/*!
	Provides a @c Tape containing a Commodore-format tape image, which is simply a timed list of downward-going zero crossings.
*/
class CommodoreTAP: public Tape {
	public:
		/*!
			Constructs a @c CommodoreTAP containing content from the file with name @c file_name.

			@throws ErrorNotCommodoreTAP if this file could not be opened and recognised as a valid Commodore-format TAP.
		*/
		CommodoreTAP(const std::string &file_name);

		enum {
			ErrorNotCommodoreTAP
		};

		// implemented to satisfy @c Tape
		bool is_at_end();

	private:
		Storage::FileHolder file_;
		void virtual_reset();
		Pulse virtual_get_next_pulse();

		bool updated_layout_;
		uint32_t file_size_;

		Pulse current_pulse_;
		bool is_at_end_ = false;
};

}
}

#endif /* CommodoreTAP_hpp */
