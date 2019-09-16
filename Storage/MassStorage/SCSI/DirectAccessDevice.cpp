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

bool DirectAccessDevice::write(const Target::CommandState &state, Target::Responder &responder) {
	if(!device_) return false;

	responder.receive_data(device_->get_block_size(), [this] (const Target::CommandState &state, Target::Responder &responder) {
		this->device_->set_block(state.address(), state.received_data());
		responder.terminate_command(Target::Responder::Status::Good);
	});

	return true;
}

bool DirectAccessDevice::read_capacity(const Target::CommandState &state, Target::Responder &responder) {
	const auto final_block = device_->get_number_of_blocks() - 1;
	const auto block_size = device_->get_block_size();
	std::vector<uint8_t> data = {
		uint8_t(final_block >> 24),
		uint8_t(final_block >> 16),
		uint8_t(final_block >> 8),
		uint8_t(final_block >> 0),

		uint8_t(block_size >> 24),
		uint8_t(block_size >> 16),
		uint8_t(block_size >> 8),
		uint8_t(block_size >> 0),
	};

	responder.send_data(std::move(data), [] (const Target::CommandState &state, Target::Responder &responder) {
		responder.terminate_command(Target::Responder::Status::Good);
	});

	return true;
}

Target::Executor::Inquiry DirectAccessDevice::inquiry_values() {
	return Inquiry("Apple", "ProFile", "1");	// All just guesses.
}

bool DirectAccessDevice::format_unit(const Target::CommandState &state, Target::Responder &responder) {
	// Formatting: immediate.
	responder.terminate_command(Target::Responder::Status::Good);
	return true;
}
