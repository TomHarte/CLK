//
//  CoCoCAS.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 07/05/2026.
//  Copyright © 2026 Thomas Harte. All rights reserved.
//

#pragma once

#include "Storage/Tape/Tape.hpp"
#include "Storage/FileHolder.hpp"
#include "Storage/Tape/PulseQueuedTape.hpp"

#include <string>

namespace Storage::Tape {

class CoCoCAS: public Tape {
public:
	CoCoCAS(const std::string &file_name);

private:
	std::string file_name_;
	std::unique_ptr<FormatSerialiser> format_serialiser() const override;

	struct Serialiser: public PulseQueuedSerialiser {
		Serialiser(const std::string &);

	private:
		void push_next_pulses() override;
		void reset() override;

		Storage::FileHolder file_;
	};
};

}
