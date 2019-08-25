//
//  DirectAccessDevice.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 22/08/2019.
//  Copyright © 2019 Thomas Harte. All rights reserved.
//

#include "DirectAccessDevice.hpp"

using namespace SCSI;

bool DirectAccessDevice::read(const Target::CommandState &state, Target::Responder &responder) {
	std::vector<uint8_t> data(512);
	for(size_t c = 0; c < 512; ++c) {
		data[c] = uint8_t(c);
	}

	responder.send_data(std::move(data), [] (const Target::CommandState &state, Target::Responder &responder) {
		responder.end_command();
	});

	return true;
}
