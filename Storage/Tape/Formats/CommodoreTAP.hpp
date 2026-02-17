//
//  CommodoreTAP.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 25/06/2016.
//  Copyright 2016 Thomas Harte. All rights reserved.
//

#pragma once

#include "Storage/Tape/Tape.hpp"
#include "Storage/FileHolder.hpp"

#include "Storage/TargetPlatforms.hpp"

#include <cstdint>
#include <string_view>

namespace Storage::Tape {

/*!
	Provides a @c Tape containing a Commodore-format tape image, which is simply a timed list of downward-going zero crossings.
*/
class CommodoreTAP: public Tape, public TargetPlatform::Distinguisher {
public:
	/*!
		Constructs a @c CommodoreTAP containing content from the file with name @c file_name.

		@throws ErrorNotCommodoreTAP if this file could not be opened and recognised as a valid Commodore-format TAP.
	*/
	CommodoreTAP(std::string_view file_name);

	enum {
		ErrorNotCommodoreTAP
	};

private:
	TargetPlatform::Type target_platforms() override;
	std::unique_ptr<FormatSerialiser> format_serialiser() const override;

	enum class FileType {
		C16, C64,
	};
	enum class Platform: uint8_t {
		C64 = 0,
		Vic20 = 1,
		C16 = 2,
	};
	enum class VideoStandard: uint8_t {
		PAL = 0,
		NTSC1 = 1,
		NTSC2 = 2,
	};

	struct Serialiser: public FormatSerialiser {
		Serialiser(std::string_view file_name, Pulse initial, bool half_waves, bool updated_layout);

	private:
		bool is_at_end() const override;
		void reset() override;
		Pulse next_pulse() override;

		Storage::FileHolder file_;
		Pulse current_pulse_;
		bool half_waves_;
		bool updated_layout_;
		bool is_at_end_ = false;
	};
	std::string file_name_;
	Pulse initial_pulse_;
	bool half_waves_;
	bool updated_layout_;
	Platform platform_;
};

}
