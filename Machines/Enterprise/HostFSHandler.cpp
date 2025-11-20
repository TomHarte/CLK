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

HostFSHandler::HostFSHandler(MemoryAccessor &accessor) : accessor_(accessor) {}

void HostFSHandler::perform(const uint8_t function, uint8_t &a, uint16_t &bc, uint16_t &de) {
	const auto set_error = [&](const EXOS::Error error) {
		a = uint8_t(error);
	};
	const auto set_c = [&](const uint8_t c) {
		bc = (bc & 0xff00) | c;
	};

	const auto read_de = [&]() {
		return accessor_.hostfs_read(de++);
	};
	const auto find_channel = [&]() {
		if(a == 255) {
			return channels_.end();
		}
		return channels_.find(a);
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
		// Emprically: C contains the unit number.
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

			// The only difference between open and create is that the former is
			// meant to append.
			const auto mode =
				function == uint8_t(EXOS::Function::CreateChannel) ?
					Storage::FileMode::Rewrite : Storage::FileMode::ReadWrite;

			set_error(EXOS::Error::NoError);
			try {
				channels_.emplace(a, bundle_->open(name, mode));
			} catch(Storage::FileHolder::Error) {
				if(mode == Storage::FileMode::ReadWrite) {
					try {
						channels_.emplace(a, bundle_->open(name, Storage::FileMode::Read));
					} catch(Storage::FileHolder::Error) {
						set_error(EXOS::Error::FileDoesNotExist);
					}
				} else {
					set_error(EXOS::Error::FileAlreadyExists);
				}
			}
		} break;

		// Page 54.
		case uint8_t(EXOS::Function::CloseChannel): {
			auto channel = find_channel();
			if(channel == channels_.end()) {
				set_error(EXOS::Error::ChannelIllegalOrDoesNotExist);
				break;
			}

			set_error(EXOS::Error::NoError);
			channels_.erase(channel);
		} break;

		// Page 55.
		case uint8_t(EXOS::Function::ReadCharacter):
			auto channel = find_channel();
			if(channel == channels_.end()) {
				set_error(EXOS::Error::ChannelIllegalOrDoesNotExist);
				break;
			}

			set_error(EXOS::Error::NoError);
			set_c(channel->second.get());
		break;
	}
}

void HostFSHandler::set_file_bundle(std::shared_ptr<Storage::FileBundle::FileBundle> bundle) {
	bundle_ = bundle;
}
