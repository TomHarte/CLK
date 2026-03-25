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

	# TO machines:

		That layout is:

			0:			start bit
			LSB...MSB:	byte content
			1 1:		stop bits.

		Each bit is composed of square waves — low, then high — of idealised duration:

			7 / 31500ths of a second, 5 times over: 0 bit
			5 / 31500ths of a second, 7 times over: 1 bit

		So baud rate is 31500 / (5 * 7) = 900.

	# MO machines:

		Basic FM encoding is used with an idealised 833µs window length; each window begins
		with a level transition after which:

			any transitions in that window: 1 bit
			no transitions: 0 bit.

		Specifically, the ROM watches for an edge to synchronise, then waits for two thirds of the
		bit length, then samples again to decide a 0 or a 1. And repeat.

*/

K7::K7(const std::string &file_name) : file_name_(file_name) {}

std::unique_ptr<FormatSerialiser> K7::format_serialiser() const {
	return std::make_unique<Serialiser>(file_name_);
}

K7::Serialiser::Serialiser(const std::string &name) : file_(name, FileMode::Read) {}

void K7::Serialiser::set_target_platforms(const TargetPlatform::Type type) {
	target_ = type;
}

void K7::Serialiser::reset() {
	file_.seek(0, Whence::SET);
}

void K7::Serialiser::push_next_pulses() {
	if(file_.eof()) {
		set_is_at_end(true);
		return;
	}

	// The TO and MO have entirely-distinct encodings.
	if(target_ == TargetPlatform::ThomsonTO) {
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

		uint8_t byte = file_.get();
		for(int c = 0; c < 8; c++) {
			post(byte & 1);
			byte >>= 1;
		}

		post(1);
		post(1);
	} else {
		const auto pulse_type = [&]() {
			current_type_ = current_type_ == Pulse::Type::High ? Pulse::Type::Low : Pulse::Type::High;
			return current_type_;
		};

		const auto post = [&](const bool bit) {
			static constexpr auto FullPulse = Time(833, 1'000'000);		// 833 µs
			static constexpr auto HalfPulse = Time(417, 1'000'000);		// 417 µs

			if(bit) {
				emplace_back(pulse_type(), HalfPulse);
				emplace_back(pulse_type(), HalfPulse);
			} else {
				emplace_back(pulse_type(), FullPulse);
			}
		};

		uint8_t byte = file_.get();
		for(int c = 0; c < 8; c++) {
			post(byte & 1);
			byte >>= 1;
		}
	}
}
