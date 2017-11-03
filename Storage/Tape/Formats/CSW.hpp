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
#include <vector>

namespace Storage {
namespace Tape {

/*!
	Provides a @c Tape containing a CSW tape image, which is a compressed 1-bit sampling.
*/
class CSW: public Tape {
	public:
		/*!
			Constructs a @c CSW containing content from the file with name @c file_name.

			@throws ErrorNotCSW if this file could not be opened and recognised as a valid CSW file.
		*/
		CSW(const char *file_name);

		enum {
			ErrorNotCSW
		};

		// implemented to satisfy @c Tape
		bool is_at_end();

	private:
		Storage::FileHolder file_;

		void virtual_reset();
		Pulse virtual_get_next_pulse();

		Pulse pulse_;
		enum CompressionType {
			RLE,
			ZRLE
		} compression_type_;

		uint8_t get_next_byte();
		uint32_t get_next_int32le();
		void invert_pulse();

		std::vector<uint8_t> source_data_;
		size_t source_data_pointer_;

		long rle_start_;
};

}
}

#endif /* CSW_hpp */
