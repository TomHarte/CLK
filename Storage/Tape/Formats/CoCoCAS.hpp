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

		// Raw shifted input from the file.
		uint16_t input_ = 0;
		int input_depth_ = 0;
		void shift();
		Storage::FileHolder file_;

		enum class State {
			PreLeadInPause,
			LeadIn,
			FlushLeadIn,
			GetBodyLength,
			Body,
			FlushBody,
		} state_ = State::PreLeadInPause;
		void set_state(const State);
		int state_length_ = 0;

	};
};

}
