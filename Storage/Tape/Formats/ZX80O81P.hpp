//
//  ZX80O.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 04/06/2017.
//  Copyright 2017 Thomas Harte. All rights reserved.
//

#pragma once

#include "../Tape.hpp"

#include "../../FileHolder.hpp"
#include "../../TargetPlatforms.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace Storage::Tape {

/*!
	Provides a @c Tape containing a ZX80-format .O tape image, which is a byte stream capture.
*/
class ZX80O81P: public Tape, public TargetPlatform::TypeDistinguisher {
public:
	/*!
		Constructs a @c ZX80O containing content from the file with name @c file_name.

		@throws ErrorNotZX80O81P if this file could not be opened and recognised as a valid ZX80-format .O.
	*/
	ZX80O81P(const std::string &file_name);

	enum {
		ErrorNotZX80O81P
	};

private:
	// TargetPlatform::TypeDistinguisher.
	TargetPlatform::Type target_platform_type() override;

	struct Serialiser: public TapeSerialiser {
		Serialiser(const std::string &file_name);
		TargetPlatform::Type target_platform_type();

	private:
		bool is_at_end() const override;
		void reset() override;
		Pulse get_next_pulse() override;
		bool has_finished_data() const;

		TargetPlatform::Type platform_type_;

		uint8_t byte_;
		int bit_pointer_;
		int wave_pointer_;
		bool is_past_silence_, has_ended_final_byte_;
		bool is_high_;

		std::vector<uint8_t> data_;
		std::size_t data_pointer_;
	} serialiser_;
};

}
