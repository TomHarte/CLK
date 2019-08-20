//
//  DirectAccessDevice.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 17/08/2019.
//  Copyright © 2019 Thomas Harte. All rights reserved.
//

#include "DirectAccessDevice.hpp"

using namespace SCSI::Target;

CommandArguments::CommandArguments(const std::vector<uint8_t> &data) : data_(data) {}

uint32_t CommandArguments::address() {
	switch(data_.size()) {
		default:	return 0;
		case 6:
			return
				(uint32_t(data_[1]) << 16) |
				(uint32_t(data_[2]) << 8) |
				uint32_t(data_[3]);
		case 10:
		case 12:
			return
				(uint32_t(data_[1]) << 24) |
				(uint32_t(data_[2]) << 16) |
				(uint32_t(data_[3]) << 8) |
				uint32_t(data_[4]);
	}
}

uint16_t CommandArguments::number_of_blocks() {
	switch(data_.size()) {
		default:	return 0;
		case 6:
			return uint16_t(data_[4]);
		case 10:
			return uint16_t((data_[7] << 8) | data_[8]);
	}
}
