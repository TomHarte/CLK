//
//  TapeUEF.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 18/01/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#ifndef TapeUEF_hpp
#define TapeUEF_hpp

#include "../PulseQueuedTape.hpp"

#include "../../TargetPlatforms.hpp"

#include <cstdint>
#include <string>
#include <zlib.h>

namespace Storage {
namespace Tape {

/*!
	Provides a @c Tape containing a UEF tape image, a slightly-convoluted description of pulses.
*/
class UEF : public PulseQueuedTape, public TargetPlatform::TypeDistinguisher {
	public:
		/*!
			Constructs a @c UEF containing content from the file with name @c file_name.

			@throws ErrorNotUEF if this file could not be opened and recognised as a valid UEF.
		*/
		UEF(const std::string &file_name);
		~UEF();

		enum {
			ErrorNotUEF
		};

	private:
		void virtual_reset();

		void set_platform_type();
		TargetPlatform::Type target_platform_type();
		TargetPlatform::Type platform_type_ = TargetPlatform::Acorn;

		gzFile file_;
		unsigned int time_base_ = 1200;
		bool is_300_baud_ = false;

		struct Chunk {
			uint16_t id;
			uint32_t length;
			z_off_t start_of_next_chunk;
		};

		bool get_next_chunk(Chunk &);
		void get_next_pulses();

		void queue_implicit_bit_pattern(uint32_t length);
		void queue_explicit_bit_pattern(uint32_t length);

		void queue_integer_gap();
		void queue_floating_point_gap();

		void queue_carrier_tone();
		void queue_carrier_tone_with_dummy();

		void queue_security_cycles();
		void queue_defined_data(uint32_t length);

		void queue_bit(int bit);
		void queue_implicit_byte(uint8_t byte);
};

}
}

#endif /* TapeUEF_hpp */
