//
//  CAS.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 25/11/2017.
//  Copyright 2017 Thomas Harte. All rights reserved.
//

#pragma once

#include "Storage/Tape/Tape.hpp"
#include "../../FileHolder.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace Storage::Tape {

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

private:
	std::unique_ptr<FormatSerialiser> format_serialiser() const override;

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

	struct Serialiser: public FormatSerialiser {
		Serialiser(const std::vector<Chunk> &chunks);

	private:
		bool is_at_end() const override;
		void reset() override;
		Pulse next_pulse() override;

		const std::vector<Chunk> &chunks_;

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
};

}
