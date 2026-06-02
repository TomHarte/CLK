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

	enum {
		ErrorBadFormat
	};

private:
	struct Block {
		std::vector<uint8_t> data;
	};
	std::vector<Block> blocks_;

	std::unique_ptr<FormatSerialiser> format_serialiser() const override;

	struct Serialiser: public PulseQueuedSerialiser {
		Serialiser(const std::vector<Block> &);

	private:
		void push_next_pulses() override;
		void reset() override;

		const std::vector<Block> &blocks_;
		std::vector<Block>::const_iterator block_;

		enum class State {
			LeadIn,
			Body,
		} state_ = State::LeadIn;
		size_t state_length_ = 0;
	};
};

}
