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

		uint32_t file_size_;
		enum class FileType {
			C16, C64,
		} type_;
		uint8_t version_;
		enum class Platform: uint8_t {
			C64 = 0,
			Vic20 = 1,
			C16 = 2,
		} platform_;
		enum class VideoStandard: uint8_t {
			PAL = 0,
			NTSC1 = 1,
			NTSC2 = 2,
		} video_;
		bool updated_layout() const {
			return version_ >= 1;
		}
		bool half_waves() const {
			return version_ >= 2;
		}
		bool double_clock() const {
			return platform_ != Platform::C16 || !half_waves();
		}

		Pulse current_pulse_;
		bool is_at_end_ = false;
	} serialiser_;
};

}
