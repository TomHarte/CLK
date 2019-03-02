//
//  CAS.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 25/11/2017.
//  Copyright 2017 Thomas Harte. All rights reserved.
//

#ifndef CAS_hpp
#define CAS_hpp

#include "../Tape.hpp"
#include "../../FileHolder.hpp"

#include <cstdint>
#include <string>
#include <vector>

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
		CAS(const std::string &file_name);

		enum {
			ErrorNotCAS
		};

		// implemented to satisfy @c Tape
		bool is_at_end();

	private:
		void virtual_reset();
		Pulse virtual_get_next_pulse();

		// Storage for the array of data blobs to transcribe into audio;
		// each chunk is preceded by a header which may be long, and is optionally
		// also preceded by a gap.
		struct Chunk {
			bool has_gap;
			bool long_header;
			std::vector<std::uint8_t> data;

			Chunk(bool has_gap, bool long_header, const std::vector<std::uint8_t> &data) :
				has_gap(has_gap), long_header(long_header), data(std::move(data)) {}
		};
		std::vector<Chunk> chunks_;

		// Tracker for active state within the file list.
		std::size_t chunk_pointer_ = 0;
		enum class Phase {
			Header,
			Bytes,
			Gap,
			EndOfFile
		} phase_ = Phase::Header;
		std::size_t distance_into_phase_ = 0;
		std::size_t distance_into_bit_ = 0;
};

}
}

#endif /* CAS_hpp */
