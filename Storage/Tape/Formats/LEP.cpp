//
//  LEP.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 19/04/2026.
//  Copyright © 2026 Thomas Harte. All rights reserved.
//

#include "LEP.hpp"

#include <cmath>

/*
	LEP files are an ungainly attempt at recording a 1bit wave with RLE compression, originating
	from the Thomson home computers.

	I've had to machine-translate documentation but as I understand it:

		* there is no header;
		* from there on, it's a sequence of signed 8-bit integers.

	Having read an 8-bit integer n, output a high level if n is positive or a low level if n is negative.
	Output for abs(n) units of time.

	Special case: if n = 0 then output the same level as previously established for a fixed 127 units of time.

	Aside: since both +127 and -127 can be stored in a byte, it's unclear to me how this special case adds anything.
	It's therefore possible that machine translation has failed here.

	On "units of time" the documentation says opaquely:
		"This duration N is expressed in the unit specified as a parameter"

	But I think it's usually or always 50µs.
*/

using namespace Storage::Tape;

LEP::LEP(const std::string &file_name) : file_name_(file_name) {}

std::unique_ptr<FormatSerialiser> LEP::format_serialiser() const {
	return std::make_unique<Serialiser>(file_name_);
}

LEP::Serialiser::Serialiser(const std::string &name) : file_(name, FileMode::Read) {
	// Empirically: a length of about 16 in a LEP means "a full pulse", i.e. 833µs or thereabouts.
	//
	// That seems to gel with the 50µs clock precision guess.
	pulse_.length.clock_rate = 20'000;
}

void LEP::Serialiser::reset() {
	file_.seek(0, Whence::SET);
}

bool LEP::Serialiser::is_at_end() const {
	return file_.eof();
}

Pulse LEP::Serialiser::next_pulse() {
	const auto duration = int8_t(file_.get());
	if(!duration) {
		pulse_.length.length = 127;
		return pulse_;
	}

	pulse_.length.length = static_cast<unsigned>(std::abs(duration));
	pulse_.type = duration < 0 ? Pulse::Type::Low : Pulse::Type::High;
	return pulse_;
}
