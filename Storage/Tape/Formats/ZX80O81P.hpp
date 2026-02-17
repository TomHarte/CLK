//
//  ZX80O.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 04/06/2017.
//  Copyright 2017 Thomas Harte. All rights reserved.
//

#pragma once

#include "Storage/Tape/Tape.hpp"

#include "Storage/FileHolder.hpp"
#include "Storage/TargetPlatforms.hpp"

#include <cstdint>
#include <string_view>
#include <vector>

namespace Storage::Tape {

/*!
	Provides a @c Tape containing a ZX80-format .O tape image, which is a byte stream capture.
*/
class ZX80O81P: public Tape, public TargetPlatform::Distinguisher {
public:
	/*!
		Constructs a @c ZX80O containing content from the file with name @c file_name.

		@throws ErrorNotZX80O81P if this file could not be opened and recognised as a valid ZX80-format .O.
	*/
	ZX80O81P(std::string_view file_name);

	enum {
		ErrorNotZX80O81P
	};

private:
	// TargetPlatform::TypeDistinguisher.
	TargetPlatform::Type target_platforms() override;
	std::unique_ptr<FormatSerialiser> format_serialiser() const override;

	struct Serialiser: public FormatSerialiser {
		Serialiser(const std::vector<uint8_t> &data);

	private:
		bool is_at_end() const override;
		void reset() override;
		Pulse next_pulse() override;
		bool has_finished_data() const;

		uint8_t byte_;
		int bit_pointer_;
		int wave_pointer_;
		bool is_past_silence_, has_ended_final_byte_;
		bool is_high_;

		const std::vector<uint8_t> &data_;
		std::size_t data_pointer_;
	};
	TargetPlatform::Type target_platforms_;
	std::vector<uint8_t> data_;
};

}
