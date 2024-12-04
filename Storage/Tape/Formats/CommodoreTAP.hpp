//
//  CommodoreTAP.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 25/06/2016.
//  Copyright 2016 Thomas Harte. All rights reserved.
//

#pragma once

#include "../Tape.hpp"
#include "../../FileHolder.hpp"

#include <cstdint>
#include <string>

namespace Storage::Tape {

/*!
	Provides a @c Tape containing a Commodore-format tape image, which is simply a timed list of downward-going zero crossings.
*/
class CommodoreTAP: public Tape {
public:
	/*!
		Constructs a @c CommodoreTAP containing content from the file with name @c file_name.

		@throws ErrorNotCommodoreTAP if this file could not be opened and recognised as a valid Commodore-format TAP.
	*/
	CommodoreTAP(const std::string &file_name);

	enum {
		ErrorNotCommodoreTAP
	};

private:
	struct Serialiser: public TapeSerialiser {
		Serialiser(const std::string &file_name);

	private:
		bool is_at_end() const override;
		void reset() override;
		Pulse next_pulse() override;

		Storage::FileHolder file_;

		bool updated_layout_;
		uint32_t file_size_;

		Pulse current_pulse_;
		bool is_at_end_ = false;
	} serialiser_;
};

}
