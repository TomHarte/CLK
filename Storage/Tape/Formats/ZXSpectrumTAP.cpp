//
//  SpectrumTAP.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 19/03/2021.
//  Copyright © 2021 Thomas Harte. All rights reserved.
//

#include "ZXSpectrumTAP.hpp"

using namespace Storage::Tape;

ZXSpectrumTAP::ZXSpectrumTAP(const std::string &file_name) :
	file_(file_name)
{
	// Check for a continuous series of blocks through to
	// file end, alternating header and data.
	uint8_t next_block_type = 0x00;
	while(true) {
		const uint16_t block_length = file_.get16le();
		const uint8_t block_type = file_.get8();
		if(file_.eof()) throw ErrorNotZXSpectrumTAP;

		if(block_type != next_block_type) {
			throw ErrorNotZXSpectrumTAP;
		}
		next_block_type ^= 0xff;

		file_.seek(block_length - 1, SEEK_CUR);
		if(file_.tell() == file_.stats().st_size) break;
	}

	virtual_reset();
}

bool ZXSpectrumTAP::is_at_end() {
	return false;
}

void ZXSpectrumTAP::virtual_reset() {
	file_.seek(0, SEEK_SET);
	block_length_ = file_.get16le();
}

Tape::Pulse ZXSpectrumTAP::virtual_get_next_pulse() {
	return Pulse();
}
