//
//  DirectAccessDevice.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 22/08/2019.
//  Copyright © 2019 Thomas Harte. All rights reserved.
//

#include "DirectAccessDevice.hpp"
#include "../../../Outputs/Log.hpp"

using namespace SCSI;

void DirectAccessDevice::set_storage(const std::shared_ptr<Storage::MassStorage::MassStorageDevice> &device) {
	device_ = device;
}

bool DirectAccessDevice::read(const Target::CommandState &state, Target::Responder &responder) {
	if(!device_) return false;

	const auto specs = state.read_write_specs();
	LOG("Read: " << std::dec << specs.number_of_blocks << " from " << specs.address);

	std::vector<uint8_t> output = device_->get_block(specs.address);
	for(uint32_t offset = 1; offset < specs.number_of_blocks; ++offset) {
		const auto next_block = device_->get_block(specs.address + offset);
		std::copy(next_block.begin(), next_block.end(), std::back_inserter(output));
	}

	responder.send_data(std::move(output), [] (const Target::CommandState &, Target::Responder &responder) {
		responder.terminate_command(Target::Responder::Status::Good);
	});

	return true;
}

bool DirectAccessDevice::write(const Target::CommandState &state, Target::Responder &responder) {
	if(!device_) return false;

	const auto specs = state.read_write_specs();
	LOG("Write: " << specs.number_of_blocks << " to " << specs.address);

	responder.receive_data(device_->get_block_size() * specs.number_of_blocks, [this, specs] (const Target::CommandState &state, Target::Responder &responder) {
		const auto received_data = state.received_data();
		const auto block_size = ssize_t(device_->get_block_size());
		for(uint32_t offset = 0; offset < specs.number_of_blocks; ++offset) {
			// TODO: clean up this gross inefficiency when std::span is standard.
			std::vector<uint8_t> sub_vector(received_data.begin() + ssize_t(offset)*block_size, received_data.begin() + ssize_t(offset+1)*block_size);
			this->device_->set_block(specs.address + offset, sub_vector);
		}
		responder.terminate_command(Target::Responder::Status::Good);
	});

	return true;
}

bool DirectAccessDevice::read_capacity(const Target::CommandState &, Target::Responder &responder) {
	if(!device_) return false;

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

	responder.send_data(std::move(data), [] (const Target::CommandState &, Target::Responder &responder) {
		responder.terminate_command(Target::Responder::Status::Good);
	});

	return true;
}

Target::Executor::Inquiry DirectAccessDevice::inquiry_values() {
	return Inquiry("Apple", "ProFile", "1");	// All just guesses.
}

bool DirectAccessDevice::format_unit(const Target::CommandState &, Target::Responder &responder) {
	if(!device_) return false;

	// Formatting: immediate.
	responder.terminate_command(Target::Responder::Status::Good);
	return true;
}
