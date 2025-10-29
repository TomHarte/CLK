//
//  CSW.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 10/07/2017.
//  Copyright 2017 Thomas Harte. All rights reserved.
//

#pragma once

#include "Storage/Tape/Tape.hpp"

#include <string>
#include <vector>
#include <zlib.h>

namespace Storage::Tape {

/*!
	Provides a @c Tape containing a CSW tape image, which is a compressed 1-bit sampling.
*/
class CSW: public Tape {
public:
	enum class CompressionType {
		RLE,
		ZRLE
	};

	/*!
		Constructs a @c CSW containing content from the file with name @c file_name.

		@throws ErrorNotCSW if this file could not be opened and recognised as a valid CSW file.
	*/
	CSW(const std::string &file_name);

	/*!
		Constructs a @c CSW containing content as specified. Does not throw.
	*/
	CSW(std::vector<uint8_t> &&data, CompressionType, bool initial_level, uint32_t sampling_rate);

	enum {
		ErrorNotCSW
	};

private:
	void set_data(std::vector<uint8_t> &&data, CompressionType type);
	std::unique_ptr<FormatSerialiser> format_serialiser() const override;

	struct Serialiser: public FormatSerialiser {
		Serialiser(const std::vector<uint8_t> &data, Pulse);

	private:
		// implemented to satisfy @c FormatSerialiser
		bool is_at_end() const override;
		void reset() override;
		Pulse next_pulse() override;

		uint8_t get_next_byte();
		uint32_t get_next_int32le();
		void invert_pulse();

		Pulse pulse_;
		const std::vector<uint8_t> &source_data_;
		std::size_t source_data_pointer_ = 0;
	};
	std::vector<uint8_t> source_data_;
	Pulse pulse_;
};

}
