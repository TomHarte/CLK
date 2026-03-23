//
//  K7.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 23/03/2026.
//  Copyright © 2026 Thomas Harte. All rights reserved.
//

#include "K7.hpp"

using namespace Storage::Tape;

namespace {

static constexpr int BitsPerByte = 11;

}

/*
	K7 files are a raw dump of source bytes that were encoded in the standard Thomson layout.

	That layout is:

		0:			start bit
		LSB...MSB:	byte content
		1 1:		stop bits.

	Each bit is a single square wave — low, then high — of idealised duration:

		7 / 31500ths of a second: 0 bit
		5 / 31500ths of a second: 1 bit
*/

K7::K7(const std::string &file_name) : file_name_(file_name) {}

std::unique_ptr<FormatSerialiser> K7::format_serialiser() const {
	return std::make_unique<Serialiser>(file_name_);
}

K7::Serialiser::Serialiser(const std::string &name) : file_(name, FileMode::Read) {
	bit_ = BitsPerByte;
}

bool K7::Serialiser::is_at_end() const {
	return bit_ == BitsPerByte && file_.eof();
}

void K7::Serialiser::reset() {
	bit_ = BitsPerByte;
	file_.seek(0, Whence::SET);
	current_pulse_.type = Pulse::High;
}

Pulse K7::Serialiser::next_pulse() {
	if(bit_ == BitsPerByte && current_pulse_.type == Pulse::High) {
		if(file_.eof()) {
			current_pulse_.length = Time(1);
			return current_pulse_;
		}

		bit_ = 0;
		byte_ = file_.get();
		current_pulse_.length.clock_rate = 31'500;
	}

	static constexpr int ZeroLength = 7;
	static constexpr int OneLength = 5;

	current_pulse_.type = current_pulse_.type == Pulse::High ? Pulse::Low : Pulse::High;
	if(current_pulse_.type == Pulse::Low) {
		switch(bit_) {
			case 0:
				current_pulse_.length.length = ZeroLength;
			break;
			case 9:
			case 10:
				current_pulse_.length.length = OneLength;
			break;

			default:
				current_pulse_.length.length = (byte_ & 1) ? OneLength : ZeroLength;
				byte_ >>= 1;
			break;
		}
	} else {
		++bit_;
	}

	return current_pulse_;
}
