//
//  HalfDuplexSerial.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 20/03/2024.
//  Copyright © 2024 Thomas Harte. All rights reserved.
//

#pragma once

namespace Archimedes {

/// Models a half-duplex serial link between two parties, framing bytes with one start bit and two stop bits.
struct HalfDuplexSerial {
	static constexpr uint16_t ShiftMask = 0b1111'1110'0000'0000;

	/// Enqueues @c value for output.
	void output(int party, uint8_t value) {
		parties_[party].output_count = 11;
		parties_[party].input = 0x7ff;
		parties_[party].output = uint16_t((value << 1) | ShiftMask);
	}

	/// @returns The last observed input.
	uint8_t input(int party) const {
		return uint8_t(parties_[party].input >> 1);
	}

	static constexpr uint8_t Receive = 1 << 0;
	static constexpr uint8_t Transmit = 1 << 1;

	/// @returns A bitmask of events that occurred during the last shift.
	uint8_t events(int party) {
		const auto result = parties_[party].events;
		parties_[party].events = 0;
		return result;
	}

	bool is_outputting(int party) const {
		return parties_[party].output_count != 11;
	}

	/// Updates the shifters on both sides of the serial link.
	void shift() {
		const uint16_t next = parties_[0].output & parties_[1].output & 1;

		for(int c = 0; c < 2; c++) {
			if(parties_[c].output_count) {
				--parties_[c].output_count;
				if(!parties_[c].output_count) {
					parties_[c].events |= Transmit;
					parties_[c].input_count = -1;
				}
				parties_[c].output = (parties_[c].output >> 1) | ShiftMask;
			} else {
				// Check for a start bit.
				if(parties_[c].input_count == -1 && !next) {
					parties_[c].input_count = 0;
				}

				// Shift in if currently observing.
				if(parties_[c].input_count >= 0 && parties_[c].input_count < 11) {
					parties_[c].input = uint16_t((parties_[c].input >> 1) | (next << 10));

					++parties_[c].input_count;
					if(parties_[c].input_count == 11) {
						parties_[c].events |= Receive;
						parties_[c].input_count = -1;
					}
				}
			}
		}
	}

private:
	struct Party {
		int output_count = 0;
		int input_count = -1;
		uint16_t output = 0xffff;
		uint16_t input = 0;
		uint8_t events = 0;
	} parties_[2];
};

static constexpr int IOCParty = 0;
static constexpr int KeyboardParty = 1;

}
