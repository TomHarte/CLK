//
//  CommodoreTAP.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 25/06/2016.
//  Copyright 2016 Thomas Harte. All rights reserved.
//

#include "CommodoreTAP.hpp"
#include <cstdio>
#include <cstring>

using namespace Storage::Tape;

CommodoreTAP::CommodoreTAP(const std::string &file_name) : Tape(serialiser_), serialiser_(file_name) {}

CommodoreTAP::Serialiser::Serialiser(const std::string &file_name) :
	file_(file_name, FileHolder::FileMode::Read)
{
	const bool is_c64 = file_.check_signature("C64-TAPE-RAW");
	file_.seek(0, SEEK_SET);
	const bool is_c16 = file_.check_signature("C16-TAPE-RAW");
	if(!is_c64 && !is_c16) {
		throw ErrorNotCommodoreTAP;
	}

	// Get and check the file version.
	version_ = file_.get8();
	if(version_ > 2) {
		throw ErrorNotCommodoreTAP;
	}

	// Read clock rate-implying bytes.
	enum Platform: uint8_t {
		C64 = 0,
		Vic20 = 1,
		C16 = 2,
	};
	const auto platform = Platform(file_.get8());
	enum VideoStandard: uint8_t {
		PAL = 0,
		NTSC1 = 1,
		NTSC2 = 2,
	};
	const auto video = VideoStandard(file_.get8());
	file_.seek(1, SEEK_CUR);

	// Read file size.
	file_size_ = file_.get32le();

	// Set up for pulse output at the requested clock rate, with each high and
	// low being half of whatever length values will be read; pretend that
	// a high pulse has just been distributed to imply that the next thing
	// that needs to happen is a length check.
	current_pulse_.length.clock_rate = static_cast<unsigned int>(
		[&] {
			switch(platform) {
				case Vic20:	return video == PAL ? 1'108'000 : 1'022'000;		// TODO: these are inexact.
				case C64:	return video == PAL ? 985'248 : 1'022'727;
				case C16:	return video == PAL ? 886'722 : 894'886;
			}
		}() * (half_waves() ? 1 : 2)
	);
	current_pulse_.type = Pulse::High;
}

void CommodoreTAP::Serialiser::reset() {
	file_.seek(0x14, SEEK_SET);
	current_pulse_.type = Pulse::High;
	is_at_end_ = false;
}

bool CommodoreTAP::Serialiser::is_at_end() const {
	return is_at_end_;
}

Storage::Tape::Pulse CommodoreTAP::Serialiser::next_pulse() {
	if(is_at_end_) {
		return current_pulse_;
	}

	const auto read_next_length = [&]() -> bool {
		uint32_t next_length;
		const uint8_t next_byte = file_.get8();
		if(!updated_layout() || next_byte > 0) {
			next_length = uint32_t(next_byte) << 3;
		} else {
			next_length = file_.get24le();
		}

		if(file_.eof()) {
			is_at_end_ = true;
			current_pulse_.length.length = current_pulse_.length.clock_rate;
			current_pulse_.type = Pulse::Zero;
			return false;
		} else {
			current_pulse_.length.length = next_length;
			return true;
		}
	};

	if(half_waves()) {
		if(read_next_length()) {
			current_pulse_.type = current_pulse_.type == Pulse::High ? Pulse::Low : Pulse::High;
		}
	} else if(current_pulse_.type == Pulse::High) {
		if(read_next_length()) {
			current_pulse_.type = Pulse::Low;
		}
	} else {
		current_pulse_.type = Pulse::High;
	}

	return current_pulse_;
}
