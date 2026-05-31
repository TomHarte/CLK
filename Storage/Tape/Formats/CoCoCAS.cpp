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
	input_ >>= 1;
	input_depth_ = std::max(input_depth_ - 1, 0);
	if(input_depth_ <= 8 && !file_.eof()) {
		input_ |= file_.get() << input_depth_;
		input_depth_ += 8;
	}
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

	const auto post_shifter = [&] {
		post_bit(input_ & 1);
		shift();
		--state_length_;
	};

	switch(state_) {
		case State::PreLeadInPause:
			serialise(0x55);
			--state_length_;
			if(!state_length_) {
				state_ = State::LeadIn;
			}
		break;

		case State::LeadIn:
			post_shifter();
			if(input_ == 0x3c55) {
				state_ = State::FlushLeadIn;
				state_length_ = 16;
			}
		break;

		case State::FlushLeadIn:
			post_shifter();
			if(!state_length_) {
				state_ = State::GetBodyLength;
				state_length_ = 8;
			}
		break;

		case State::GetBodyLength:
			post_shifter();
			if(!state_length_) {
				state_ = State::Body;
				state_length_ = (1 + (input_ & 0xff)) * 8;
			}
		break;

		case State::Body: {
			post_shifter();
			if(!state_length_) {
				state_ = State::FlushBody;
				state_length_ = 8;
			}
		}
		break;

		case State::FlushBody:
			post_shifter();
			if(!state_length_) {
				set_pre_lead_in_pause();
			}
		break;
	}
}

void CoCoCAS::Serialiser::reset() {
	set_pre_lead_in_pause();
	file_.seek(0, Whence::SET);
}

void CoCoCAS::Serialiser::set_pre_lead_in_pause() {
	state_ = State::PreLeadInPause;
	state_length_ = 150;
}
