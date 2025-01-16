//
//  TapeUEF.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 18/01/2016.
//  Copyright 2016 Thomas Harte. All rights reserved.
//

#pragma once

#include "../PulseQueuedTape.hpp"

#include "../../TargetPlatforms.hpp"

#include <cstdint>
#include <string>
#include <zlib.h>

namespace Storage::Tape {

/*!
	Provides a @c Tape containing a UEF tape image, a slightly-convoluted description of pulses.
*/
class UEF : public Tape, public TargetPlatform::Distinguisher {
public:
	/*!
		Constructs a @c UEF containing content from the file with name @c file_name.

		@throws ErrorNotUEF if this file could not be opened and recognised as a valid UEF.
	*/
	UEF(const std::string &file_name);

	enum {
		ErrorNotUEF
	};

private:
	TargetPlatform::Type target_platforms() override;

	struct Serialiser: public PulseQueuedSerialiser {
		Serialiser(const std::string &file_name);
		~Serialiser();

		TargetPlatform::Type target_platform_type();

	private:
		void reset() override;

		void set_platform_type();
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
		void push_next_pulses() override;

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
	} serialiser_;
};

}
