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

CoCoCAS::Serialiser::Serialiser(const std::string &name) : file_(name, FileMode::Read) {}

void CoCoCAS::Serialiser::push_next_pulses() {
	// Dumb: no enforced gaps between blocks.
	uint8_t next = file_.get();

	for(int c = 0; c < 8; c++) {
		// Generate a single wave of either 1200Hz (for a 0) or 2400Hz tone (for a 1).
		const Time length(
			1,
			next & 0x80 ? 4800 : 2400
		);
		next <<= 1;

		emplace_back(Pulse::Low, length);
		emplace_back(Pulse::High, length);
	}
}

void CoCoCAS::Serialiser::reset() {
	file_.seek(0, Whence::SET);
}
