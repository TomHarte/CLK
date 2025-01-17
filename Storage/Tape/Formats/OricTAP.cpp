//
//  OricTAP.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 10/10/2016.
//  Copyright 2016 Thomas Harte. All rights reserved.
//

#include "OricTAP.hpp"

#include <sys/stat.h>

using namespace Storage::Tape;

OricTAP::OricTAP(const std::string &file_name) : file_name_(file_name) {
	format_serialiser();
}

std::unique_ptr<FormatSerialiser> OricTAP::format_serialiser() const {
	return std::make_unique<Serialiser>(file_name_);
}

OricTAP::Serialiser::Serialiser(const std::string &file_name) :
	file_(file_name, FileHolder::FileMode::Read)
{
	// Check for a sequence of at least three 0x16s followed by a 0x24.
	while(true) {
		const uint8_t next = file_.get8();
		if(next != 0x16 && next != 0x24) {
			throw ErrorNotOricTAP;
		}
		if(next == 0x24) {
			if(file_.tell() < 4) {
				throw ErrorNotOricTAP;
			}
			break;
		}
	}

	// Rewind and start again.
	reset();
}

void OricTAP::Serialiser::reset() {
	file_.seek(0, SEEK_SET);
	bit_count_ = 13;
	phase_ = next_phase_ = LeadIn;
	phase_counter_ = 0;
	pulse_counter_ = 0;
}

Pulse OricTAP::Serialiser::next_pulse() {
	// Each byte byte is written as 13 bits: 0, eight bits of data, parity, three 1s.
	if(bit_count_ == 13) {
		if(next_phase_ != phase_) {
			phase_ = next_phase_;
			phase_counter_ = 0;
		}

		bit_count_ = 0;
		uint8_t next_byte = 0;
		switch(phase_) {
			case LeadIn:
				next_byte = phase_counter_ < 258 ? 0x16 : 0x24;
				phase_counter_++;
				if(phase_counter_ == 259) {	// 256 artificial bytes plus the three in the file = 259
					while(1) {
						if(file_.get8() != 0x16) break;
					}
					next_phase_ = Header;
				}
			break;

			case Header:
				// Counts are relative to:
				// [0, 1]:		"two bytes unused" (on the Oric 1)
				// 2:			program type
				// 3:			auto indicator
				// [4, 5]:		end address of data
				// [6, 7]:		start address of data
				// 8:			"unused" (on the Oric 1)
				// [9...]:		filename, up to NULL byte
				next_byte = file_.get8();

				if(phase_counter_ == 4)	data_end_address_ = uint16_t(next_byte << 8);
				if(phase_counter_ == 5)	data_end_address_ |= next_byte;
				if(phase_counter_ == 6)	data_start_address_ = uint16_t(next_byte << 8);
				if(phase_counter_ == 7)	data_start_address_ |= next_byte;

				if(phase_counter_ >= 9 && !next_byte) {	// advance after the filename-ending NULL byte
					next_phase_ = Gap;
				}
				if(file_.eof()) {
					next_phase_ = End;
				}
				phase_counter_++;
			break;

			case Gap:
				phase_counter_++;
				if(phase_counter_ == 8) {
					next_phase_ = Data;
				}
			break;

			case Data:
				next_byte = file_.get8();
				phase_counter_++;
				if(phase_counter_ >= (data_end_address_ - data_start_address_)+1) {
					if(next_byte == 0x16) {
						next_phase_ = LeadIn;
					}
					else if(file_.eof()) {
						next_phase_ = End;
					}
				}
			break;

			case End:
			break;
		}

		uint8_t parity = next_byte;
		parity ^= (parity >> 4);
		parity ^= (parity >> 2);
		parity ^= (parity >> 1);
		current_value_ = uint16_t((uint16_t(next_byte) << 1) | ((parity&1) << 9) | (7 << 10));
	}

	// In slow mode, a 0 is 4 periods of 1200 Hz, a 1 is 8 periods at 2400 Hz.
	// In fast mode, a 1 is a single period of 2400 Hz, a 0 is a 2400 Hz pulse followed by a 1200 Hz pulse.
	// This code models fast mode.
	Pulse pulse;
	pulse.length.clock_rate = 4800;
	int next_bit;

	switch(phase_) {
		case End:
			pulse.type = Pulse::Zero;
			pulse.length.length = 4800;
		return pulse;

		case Gap:
			bit_count_ = 13;
			pulse.type = (phase_counter_&1) ? Pulse::Low : Pulse::High;
			pulse.length.length = 100;
		return pulse;

		default:
			next_bit = current_value_ & 1;
		break;
	}

	if(next_bit) {
		pulse.length.length = 1;
	} else {
		pulse.length.length = pulse_counter_ ? 2 : 1;
	}
	pulse.type = pulse_counter_ ? Pulse::High : Pulse::Low;	// TODO

	pulse_counter_ ^= 1;
	if(!pulse_counter_) {
		current_value_ >>= 1;
		bit_count_++;
	}
	return pulse;
}

bool OricTAP::Serialiser::is_at_end() const {
	return phase_ == End;
}
