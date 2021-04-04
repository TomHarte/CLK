//
//  SpectrumTAP.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 19/03/2021.
//  Copyright © 2021 Thomas Harte. All rights reserved.
//

#include "ZXSpectrumTAP.hpp"

using namespace Storage::Tape;

/*
	The understanding of idiomatic Spectrum data encoding below
	is taken from the TZX specifications at
	https://worldofspectrum.net/features/TZXformat.html ;
	specifics of the TAP encoding were gained from
	https://sinclair.wiki.zxnet.co.uk/wiki/TAP_format
*/

ZXSpectrumTAP::ZXSpectrumTAP(const std::string &file_name) :
	file_(file_name)
{
	// Check for a continuous series of blocks through to
	// exactly file end.
	//
	// To consider: could also check those blocks of type 0
	// and type ff for valid checksums?
	while(true) {
		const uint16_t block_length = file_.get16le();
		if(file_.eof()) throw ErrorNotZXSpectrumTAP;

		file_.seek(block_length, SEEK_CUR);
		if(file_.tell() == file_.stats().st_size) break;
	}

	virtual_reset();
}

bool ZXSpectrumTAP::is_at_end() {
	return file_.tell() == file_.stats().st_size && phase_ == Phase::Gap;
}

void ZXSpectrumTAP::virtual_reset() {
	file_.seek(0, SEEK_SET);
	read_next_block();
}

Tape::Pulse ZXSpectrumTAP::virtual_get_next_pulse() {
	// Adopt a general pattern of high then low.
	Pulse pulse;
	pulse.type = (distance_into_phase_ & 1) ? Pulse::Type::High : Pulse::Type::Low;

	switch(phase_) {
		default: break;

		case Phase::PilotTone: {
			// Output: pulses of length 2168;
			// 8063 pulses if block type is 0, otherwise 3223;
			// then a 667-length pulse followed by a 735-length pulse.

			pulse.length = Time(271, 437'500);	// i.e. 2168 / 3'500'000
			++distance_into_phase_;

			// Check whether in the last two.
			if(distance_into_phase_ >= (block_type_ ? 8063 : 3223)) {
				pulse.length = (distance_into_phase_ & 1) ? Time(667, 3'500'000) : Time(735, 3'500'000);

				// Check whether this is the last one.
				if(distance_into_phase_ == (block_type_ ? 8064 : 3224)) {
					distance_into_phase_ = 0;
					phase_ = Phase::Data;
				}
			}
		} break;

		case Phase::Data: {
			// Output two pulses of length 855 for a 0; two of length 1710 for a 1,
			// from MSB to LSB.
			pulse.length = (data_byte_ & 0x80) ? Time(1710, 3'500'000) : Time(855, 3'500'000);
			++distance_into_phase_;

			if(!(distance_into_phase_ & 1)) {
				data_byte_ <<= 1;
			}

			if(!(distance_into_phase_ & 15)) {
				if((distance_into_phase_ >> 4) == block_length_) {
					if(block_type_) {
						distance_into_phase_ = 0;
						phase_ = Phase::Gap;
					} else {
						read_next_block();
					}
				} else {
					data_byte_ = file_.get8();
				}
			}
		} break;

		case Phase::Gap:
			Pulse gap;
			gap.type = Pulse::Type::Zero;
			gap.length = Time(1);

			read_next_block();
		return gap;
	}

	return pulse;
}

void ZXSpectrumTAP::read_next_block() {
	if(file_.tell() == file_.stats().st_size) {
		phase_ = Phase::Gap;
	} else {
		block_length_ = file_.get16le();
		data_byte_ = block_type_ = file_.get8();
		phase_ = Phase::PilotTone;
	}
	distance_into_phase_ = 0;
}
