//
//  MSX.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 26/12/2017.
//  Copyright 2017 Thomas Harte. All rights reserved.
//

#ifndef Storage_Tape_Parsers_MSX_hpp
#define Storage_Tape_Parsers_MSX_hpp

#include "../Tape.hpp"

#include <memory>
#include <cstdint>

namespace Storage {
namespace Tape {
namespace MSX {

class Parser {
	public:
		struct FileSpeed {
			uint8_t minimum_start_bit_duration;			// i.e. LOWLIM
			uint8_t low_high_disrimination_duration;	// i.e. WINWID
		};

		/*!
			Finds the next header from the tape, determining constants for the
			speed of file expected ahead.

			Attempts exactly to duplicate the MSX's TAPION function.

			@param tape_player The tape player containing the tape to search.
			@returns An instance of FileSpeed if a header is found before the end of the tape;
				@c nullptr otherwise.
		*/
		static std::unique_ptr<FileSpeed> find_header(Storage::Tape::BinaryTapePlayer &tape_player);

		/*!
			Attempts to read the next byte from the cassette, with data encoded
			at the rate as defined by @c speed.

			Attempts exactly to duplicate the MSX's TAPIN function.

			@returns A value in the range 0-255 if a byte is found before the end of the tape;
				-1 otherwise.
		*/
		static int get_byte(const FileSpeed &speed, Storage::Tape::BinaryTapePlayer &tape_player);
};

}
}
}

#endif /* Storage_Tape_Parsers_MSX_hpp */
