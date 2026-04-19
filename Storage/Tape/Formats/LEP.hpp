//
//  LEP.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 19/04/2026.
//  Copyright © 2026 Thomas Harte. All rights reserved.
//

#pragma once

#include "Storage/FileHolder.hpp"
#include "Storage/Tape/Tape.hpp"
#include <string>

namespace Storage::Tape {

class LEP: public Tape {
public:
	LEP(const std::string &file_name);

private:
	std::unique_ptr<FormatSerialiser> format_serialiser() const override;

	struct Serialiser: public FormatSerialiser {
		Serialiser(const std::string &);

		bool is_at_end() const override;
		void reset() override;
		Pulse next_pulse() override;

	private:
		Storage::FileHolder file_;
		Pulse pulse_;
	};
	std::string file_name_;
};

}
