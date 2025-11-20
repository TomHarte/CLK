//
//  HostFSHandler.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 20/11/2025.
//  Copyright © 2025 Thomas Harte. All rights reserved.
//

#include "HostFSHandler.hpp"
#include "EXOSCodes.hpp"

using namespace Enterprise;

HostFSHandler::HostFSHandler(uint8_t *ram) : ram_(ram) {}

void HostFSHandler::perform(const uint8_t function, uint8_t &a, uint16_t &bc, uint16_t &de) {
	const auto set_error = [&](const EXOS::Error error) {
		a = uint8_t(error);
	};
	const auto read_de = [&]() {
		return ram_[de++];
	};

	switch(function) {
		default:
			printf("UNIMPLEMENTED function %d\n", function);
		break;

		case uint8_t(EXOS::DeviceDescriptorFunction::Initialise):
			channels_.clear();
			set_error(EXOS::Error::NoError);
		break;

		// Page 54.
		case uint8_t(EXOS::Function::OpenChannel):
		case uint8_t(EXOS::Function::CreateChannel): {
			if(a == 255) {
				set_error(EXOS::Error::ChannelIllegalOrDoesNotExist);
				break;
			}

			// Get name.
			uint8_t length = read_de();
			std::string name;
			while(length--) {
				name.push_back(char(read_de()));
			}

			printf("Should open %s\n", name.c_str());
		} break;
	}

	(void)bc;
}

void HostFSHandler::set_file_bundle(std::shared_ptr<Storage::FileBundle::FileBundle> bundle) {
	bundle_ = bundle;
}
