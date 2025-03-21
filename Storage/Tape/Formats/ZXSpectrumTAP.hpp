//
//  SpectrumTAP.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 19/03/2021.
//  Copyright © 2021 Thomas Harte. All rights reserved.
//

#pragma once

#include "Storage/Tape/Tape.hpp"
#include "Storage/FileHolder.hpp"

#include <cstdint>
#include <string>

namespace Storage::Tape {

/*!
	Provides a @c Tape containing an Spectrum-format tape image, which contains a series of
	header and data blocks.
*/
class ZXSpectrumTAP: public Tape {
public:
	/*!
		Constructs a @c ZXSpectrumTAP containing content from the file with name @c file_name.

		@throws ErrorNotZXSpectrumTAP if this file could not be opened and recognised as a valid Spectrum-format TAP.
	*/
	ZXSpectrumTAP(const std::string &file_name);

	enum {
		ErrorNotZXSpectrumTAP
	};

private:
	std::unique_ptr<FormatSerialiser> format_serialiser() const override;

	struct Serialiser: public FormatSerialiser {
		Serialiser(const std::string &file_name);
	private:
		Storage::FileHolder file_;

		uint16_t block_length_ = 0;
		uint8_t block_type_ = 0;
		uint8_t data_byte_ = 0;
		enum Phase {
			PilotTone,
			Data,
			Gap
		} phase_ = Phase::PilotTone;
		int distance_into_phase_ = 0;
		void read_next_block();

		// Implemented to satisfy @c Tape.
		bool is_at_end() const override;
		void reset() override;
		Pulse next_pulse() override;
	};
	std::string file_name_;
};

}
