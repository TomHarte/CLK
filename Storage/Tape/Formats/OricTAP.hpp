//
//  OricTAP.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 10/10/2016.
//  Copyright 2016 Thomas Harte. All rights reserved.
//

#pragma once

#include "../Tape.hpp"
#include "../../FileHolder.hpp"

#include <cstdint>
#include <string>

namespace Storage::Tape {

/*!
	Provides a @c Tape containing an Oric-format tape image, which is a byte stream capture.
*/
class OricTAP: public Tape {
public:
	/*!
		Constructs an @c OricTAP containing content from the file with name @c file_name.

		@throws ErrorNotOricTAP if this file could not be opened and recognised as a valid Oric-format TAP.
	*/
	OricTAP(const std::string &file_name);

	enum {
		ErrorNotOricTAP
	};

private:
	std::unique_ptr<FormatSerialiser> format_serialiser() const override;

	struct Serialiser: public FormatSerialiser {
		Serialiser(const std::string &file_name);

	private:
		bool is_at_end() const override;
		void reset() override;
		Pulse next_pulse() override;

		Storage::FileHolder file_;

		// byte serialisation and output
		uint16_t current_value_;
		int bit_count_;
		int pulse_counter_;

		enum Phase {
			LeadIn, Header, Data, Gap, End
		} phase_, next_phase_;
		int phase_counter_;
		uint16_t data_end_address_, data_start_address_;
	};
	std::string file_name_;
};

}
