//
//  CoCoCAS.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 07/05/2026.
//  Copyright © 2026 Thomas Harte. All rights reserved.
//

#include "CoCoCAS.hpp"

using namespace Storage::Tape;

/*
	[CoCo-style] CAS files are a raw dump of source bytes as per Microsoft's 6809 BASIC and
	the Tandy (or Dragon) encoding.

	It is therefore very similar to the Thomson K7 file format.

*/

CoCoCAS::CoCoCAS(const std::string &file_name) : file_name_(file_name) {
	// TODO: reject unless at least one normative CoCo-esque block is within the image.
	// CAS is not an unambiguous extension.
}

std::unique_ptr<FormatSerialiser> CoCoCAS::format_serialiser() const {
	return std::make_unique<Serialiser>(file_name_);
}

CoCoCAS::Serialiser::Serialiser(const std::string &name) : file_(name, FileMode::Read) {
	reset();
}

// CAS files are weird:
//
// Many of them are purely byte-aligned captures of the original in-memory data that was written to tape,
// complete with heavily-abbreviated sync periods. So if you're not doing a ROM-hack load then there needs
// to be some additional level of interpretation to force longer sync periods so that the machine can keep up.
//
// Others appear to be real bit captures from the original tape that are not necessarily byte-aligned and
// which may or may not have abbreviated sync periods (in at least one case, it looks like the transcriber
// spotted a period that was byte-aligned while skipping others that weren't).
//
// Hence the additional level of detachment here:
//
//	(1) bytes from the tape go through a shifter;
//	(2) ROM-style structure, if recognised, invites forced interblock syncs.
void CoCoCAS::Serialiser::shift() {
	if(input_depth_ <= 8 && !file_.eof()) {
		input_ |= file_.get() << input_depth_;
		input_depth_ += 8;
	}
	input_ >>= 1;
	--input_depth_;
}

void CoCoCAS::Serialiser::push_next_pulses() {
	const auto post_bit = [&](const bool bit) {
		// Generate a single wave of either 1200Hz (for a 0) or 2400Hz tone (for a 1).
		const Time length(
			1,
			bit & 0x01 ? 4800 : 2400
		);
		emplace_back(Pulse::Low, length);
		emplace_back(Pulse::High, length);
	};

	const auto serialise = [&](uint8_t next) {
		for(int c = 0; c < 8; c++) {
			post_bit(next & 1);
			next >>= 1;
		}
	};

	switch(state_) {
		case State::PreLeadInPause:
			serialise(0x55);
			--state_length_;
			if(!state_length_) {
				set_state(State::LeadIn);
			}
		break;

		case State::LeadIn:
			if(input_ == 0x3c55) {
				set_state(State::FlushLeadIn);
			} else {
				post_bit(input_ & 1);
				shift();
			}
		break;

		case State::FlushLeadIn:
			post_bit(input_ & 1);
			shift();
			--state_length_;
			if(!state_length_) {
				set_state(State::GetBodyLength);
			}
		break;

		case State::GetBodyLength:
			post_bit(input_ & 1);
			shift();
			--state_length_;
			if(!state_length_) {
				state_length_ = (1 + (input_ & 0xff)) * 8;
				state_ = State::Body;
			}
		break;

		case State::Body: {
			post_bit(input_ & 1);
			shift();
			--state_length_;
			if(!state_length_) {
				set_state(State::FlushBody);
			}
		}
		break;

		case State::FlushBody:
			post_bit(input_ & 1);
			input_ >>= 1;
			--state_length_;
			if(!state_length_) {
				set_state(State::GetBodyLength);
			}
		break;
	}
}

void CoCoCAS::Serialiser::reset() {
	// Add 1s of blank before the tape begins.
	set_state(State::PreLeadInPause);
	file_.seek(0, Whence::SET);
}

void CoCoCAS::Serialiser::set_state(const State state) {
	state_ = state;
	switch(state) {
		case State::Body:			state_length_ = -1;		break;
		case State::FlushLeadIn:	state_length_ = 15;		break;
		case State::GetBodyLength:	state_length_ = 9;		break;
		case State::LeadIn:			state_length_ = 0;		break;
		case State::PreLeadInPause:	state_length_ = 150;	break;
		default: __builtin_unreachable();
	}
}
