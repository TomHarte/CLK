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

void CoCoCAS::Serialiser::push_next_pulses() {
	const auto serialise = [&](uint8_t next) {
		for(int c = 0; c < 8; c++) {
			// Generate a single wave of either 1200Hz (for a 0) or 2400Hz tone (for a 1).
			const Time length(
				1,
				next & 0x01 ? 4800 : 2400
			);
			next >>= 1;

			emplace_back(Pulse::Low, length);
			emplace_back(Pulse::High, length);
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

		case State::LeadIn: {
			const uint8_t next = file_.get();
			serialise(next);

			if(next == 0x3c) {
				printf("Sync\n");
				set_state(State::Body);
			}
		}
		break;

		case State::Body: {
			const uint8_t next = file_.get();
			serialise(next);

			--state_length_;
			if(state_length_ == -3) {
				state_length_ = next + 1;
				printf("Grabbed length %d\n", state_length_);
			} else if(!state_length_) {
				printf("Ended block\n");
				set_state(State::PreLeadInPause);
			}
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
		case State::LeadIn:			state_length_ = 0;		break;
		case State::PreLeadInPause:	state_length_ = 150;	break;
		default: __builtin_unreachable();
	}
}
