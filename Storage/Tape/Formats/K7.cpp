//
//  K7.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 23/03/2026.
//  Copyright © 2026 Thomas Harte. All rights reserved.
//

#include "K7.hpp"

using namespace Storage::Tape;

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

		There are not start and stop bits; bits are stored MSB first.

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
	state_ = State::Seeking;
}

uint8_t K7::Serialiser::next() {
	// The state machine below forces gaps between files; the K7 file format is predicated on emulator hacks
	// underneath — it assumes a high-level capturing of the relevant ROM routines.
	//
	// This attempts to ameliorate for that.
	switch(state_) {
		case State::Seeking: {
			const uint8_t byte = file_.get();
			byte_history_ = uint16_t((byte_history_ << 8) | byte);
			if(byte_history_ == 0x3c5a) {
				state_ = State::Header;
				state_length_ = 0;
			}
			return byte;
		} break;

		case State::Header: {
			const uint8_t byte = file_.get();
			++state_length_;
			if(state_length_ == 2) {
				state_ = State::Body;
				state_length_ = byte - 1;		// Because the test in ::Body is posthoc.
			}
			return byte;
		} break;

		case State::Body: {
			const uint8_t byte = file_.get();
			--state_length_;
			if(!state_length_) {
				state_ = State::PostBodyPause;
				state_length_ = 50;				// Arbitrary; selected to be 'long enough'.
			}
			return byte;
		} break;

		case State::PostBodyPause: {
			--state_length_;
			if(!state_length_) {
				state_ = State::Seeking;
			}
			return 0x01;						// Almost arbitrary: matches the standard synchronisation byte.
		} break;

		default: __builtin_unreachable();
	}
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

		uint8_t byte = next();
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
			// TODO: could probably shave 1/4 off timings here and just be a tape playing quickly?
			static constexpr auto FullPulse = Time(17, 20408);		// ~833 µs.
			static constexpr auto HalfPulse = Time(17, 40816);		// ~417 µs.

			if(bit) {
				emplace_back(pulse_type(), HalfPulse);
				emplace_back(pulse_type(), HalfPulse);
			} else {
				emplace_back(pulse_type(), FullPulse);
			}
		};

		uint8_t byte = next();
		for(int c = 0; c < 8; c++) {
			post(byte & 0x80);
			byte <<= 1;
		}
	}
}
