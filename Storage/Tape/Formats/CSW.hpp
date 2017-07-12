//
//  CSW.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 10/07/2017.
//  Copyright © 2017 Thomas Harte. All rights reserved.
//

#ifndef CSW_hpp
#define CSW_hpp

#include "../Tape.hpp"
#include "../../FileHolder.hpp"

#include <zlib.h>

namespace Storage {
namespace Tape {

/*!
	Provides a @c Tape containing a CSW tape image, which is a compressed 1-bit sampling.
*/
class CSW: public Tape, public Storage::FileHolder {
	public:
		/*!
			Constructs a @c CSW containing content from the file with name @c file_name.

			@throws ErrorNotCSW if this file could not be opened and recognised as a valid CSW file.
		*/
		CSW(const char *file_name);
		~CSW();

		enum {
			ErrorNotCSW
		};

		// implemented to satisfy @c Tape
		bool is_at_end();

	private:
		void virtual_reset();
		Pulse virtual_get_next_pulse();

		Pulse pulse_;
		enum CompressionType {
			RLE,
			ZRLE
		} compression_type_;
		uint32_t number_of_waves_;
		z_stream inflation_stream_;
};

}
}

#endif /* CSW_hpp */
