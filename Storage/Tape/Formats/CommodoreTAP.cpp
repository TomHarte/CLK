//
//  CommodoreTAP.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 25/06/2016.
//  Copyright 2016 Thomas Harte. All rights reserved.
//

#include "CommodoreTAP.hpp"
#include <cstdio>
#include <cstring>

using namespace Storage::Tape;

CommodoreTAP::CommodoreTAP(const std::string &file_name) :
	file_(file_name)
{
	if(!file_.check_signature("C64-TAPE-RAW"))
		throw ErrorNotCommodoreTAP;

	// check the file version
	switch(file_.get8()) {
		case 0:		updated_layout_ = false;	break;
		case 1:		updated_layout_ = true;		break;
		default:	throw ErrorNotCommodoreTAP;
	}

	// skip reserved bytes
	file_.seek(3, SEEK_CUR);

	// read file size
	file_size_ = file_.get32le();

	// set up for pulse output at the PAL clock rate, with each high and
	// low being half of whatever length values will be read; pretend that
	// a high pulse has just been distributed to imply that the next thing
	// that needs to happen is a length check
	current_pulse_.length.clock_rate = 985248 * 2;
	current_pulse_.type = Pulse::High;
}

void CommodoreTAP::virtual_reset() {
	file_.seek(0x14, SEEK_SET);
	current_pulse_.type = Pulse::High;
	is_at_end_ = false;
}

bool CommodoreTAP::is_at_end() {
	return is_at_end_;
}

Storage::Tape::Tape::Pulse CommodoreTAP::virtual_get_next_pulse() {
	if(is_at_end_) {
		return current_pulse_;
	}

	if(current_pulse_.type == Pulse::High) {
		uint32_t next_length;
		uint8_t next_byte = file_.get8();
		if(!updated_layout_ || next_byte > 0) {
			next_length = (uint32_t)next_byte << 3;
		} else {
			next_length = file_.get24le();
		}

		if(file_.eof()) {
			is_at_end_ = true;
			current_pulse_.length.length = current_pulse_.length.clock_rate;
			current_pulse_.type = Pulse::Zero;
		} else {
			current_pulse_.length.length = next_length;
			current_pulse_.type = Pulse::Low;
		}
	} else {
		current_pulse_.type = Pulse::High;
	}

	return current_pulse_;
}
