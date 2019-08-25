//
//  DirectAccessDevice.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 22/08/2019.
//  Copyright © 2019 Thomas Harte. All rights reserved.
//

#include "DirectAccessDevice.hpp"

using namespace SCSI;


void DirectAccessDevice::set_storage(const std::shared_ptr<Storage::MassStorage::MassStorageDevice> &device) {
	device_ = device;
}

bool DirectAccessDevice::read(const Target::CommandState &state, Target::Responder &responder) {
	if(!device_) return false;

	responder.send_data(device_->get_block(state.address()), [] (const Target::CommandState &state, Target::Responder &responder) {
		responder.end_command();
	});

	return true;
}
