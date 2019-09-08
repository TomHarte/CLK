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
		responder.terminate_command(Target::Responder::Status::Good);
	});

	return true;
}

bool DirectAccessDevice::inquiry(const Target::CommandState &state, Target::Responder &responder) {
	if(!device_) return false;

	std::vector<uint8_t> response = {
		0x00,	/* Peripheral device type: directly addressible. */
		0x00,	/* Non-removeable (0x80 = removeable). */
		0x00,	/* Version: does not claim conformance to any standard. */
		0x00,	/* Response data format: 0 for SCSI-1? */
		0x00,	/* Additional length. */
	};

	const auto allocated_bytes = state.allocated_inquiry_bytes();
	if(allocated_bytes < response.size()) {
		response.resize(allocated_bytes);
	}

	responder.send_data(std::move(response), [] (const Target::CommandState &state, Target::Responder &responder) {
		responder.terminate_command(Target::Responder::Status::Good);
	});

	return true;
}
