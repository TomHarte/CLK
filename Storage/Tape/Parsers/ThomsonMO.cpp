//
//  ThomsonMO.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 25/03/2026.
//  Copyright © 2026 Thomas Harte. All rights reserved.
//

#include "ThomsonMO.hpp"

using namespace Storage::Tape::Thomson::MO;

std::optional<bool> Parser::bit(Storage::Tape::TapeSerialiser &serialiser) {
	Pulse pulse;

	// Find next transition.
	while(!serialiser.is_at_end()) {
		pulse = serialiser.next_pulse();
		if(pulse.type != last_type_ && pulse.type != Pulse::Type::Zero) {
			break;
		}
	}
	if(serialiser.is_at_end()) return std::nullopt;

	// Advance at least 555µs and sample again.
	float time = 0.0f;
	while(!serialiser.is_at_end()) {
		time += pulse.length.get<float>();
		if(time >= 0.000'555) break;
		pulse = serialiser.next_pulse();
	}
	if(serialiser.is_at_end()) return std::nullopt;

	const bool result = pulse.type == last_type_;
	last_type_ = pulse.type;
	return result;
}

//std::optional<uint8_t> Parser::byte(Storage::Tape::TapeSerialiser &serialiser) {
//
//}
