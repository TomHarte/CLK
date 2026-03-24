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

	Each bit is composed of square waves — low, then high — of idealised duration:

		7 / 31500ths of a second, 5 times over: 0 bit
		5 / 31500ths of a second, 7 times over: 1 bit

	So baud rate is 31500 / (5 * 7) = 900.
*/

K7::K7(const std::string &file_name) : file_name_(file_name) {}

std::unique_ptr<FormatSerialiser> K7::format_serialiser() const {
	return std::make_unique<Serialiser>(file_name_);
}

K7::Serialiser::Serialiser(const std::string &name) : file_(name, FileMode::Read) {
	bit_ = BitsPerByte;
}

void K7::Serialiser::reset() {
	file_.seek(0, Whence::SET);
}

void K7::Serialiser::push_next_pulses() {
	if(file_.eof()) {
		set_is_at_end(true);
		return;
	}

	static constexpr int LengthDenominator = 31'500 * 2;

	static constexpr auto ZeroLength = Time(7, LengthDenominator);
	static constexpr int ZeroRepetitions = 5;

	static constexpr auto OneLength = Time(5, LengthDenominator);
	static constexpr int OneRepetitions = 7;

	const auto post = [&](const bool bit) {
		const auto output = [&](const Time length, const int repetitions) {
			for(int c = 0; c < repetitions; c++) {
				emplace_back(Pulse::Low, length);
				emplace_back(Pulse::High, length);
			}
		};
		if(bit) {
			output(OneLength, OneRepetitions);
		} else {
			output(ZeroLength, ZeroRepetitions);
		}
	};

	post(0);

	byte_ = file_.get();
	for(int c = 0; c < 8; c++) {
		post(byte_ & 1);
		byte_ >>= 1;
	}

	post(1);
	post(1);
}
