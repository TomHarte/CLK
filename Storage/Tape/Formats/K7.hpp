//
//  K7.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 23/03/2026.
//  Copyright © 2026 Thomas Harte. All rights reserved.
//

#pragma once

#include "Storage/Tape/Tape.hpp"
#include "Storage/FileHolder.hpp"
#include "Storage/Tape/PulseQueuedTape.hpp"

#include <memory>
#include <string>

namespace Storage::Tape {

/*!
	Provides a @c Tape containing a CSW tape image, which is a compressed 1-bit sampling.
*/
class K7: public Tape {
public:
	K7(const std::string &file_name);

private:
	std::unique_ptr<FormatSerialiser> format_serialiser() const override;

	struct Serialiser: public PulseQueuedSerialiser, public TargetPlatform::Recipient {
		Serialiser(const std::string &);

	private:
		void push_next_pulses() override;
		void reset() override;
		void set_target_platforms(TargetPlatform::Type) override;

		Storage::FileHolder file_;
		Pulse::Type current_type_;
		TargetPlatform::Type target_;
	};
	std::string file_name_;
};

}
