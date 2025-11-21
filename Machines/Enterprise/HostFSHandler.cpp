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
			printf("UNIMPLEMENTED function %d with A:%02x BC:%04x DE:%04x\n", function, a, bc, de);
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

			try {
				channels_.emplace(a, bundle_->open(name, mode));
				set_error(EXOS::Error::NoError);
			} catch(Storage::FileHolder::Error) {
				if(mode == Storage::FileMode::ReadWrite) {
					try {
						channels_.emplace(a, bundle_->open(name, Storage::FileMode::Read));
						set_error(EXOS::Error::NoError);
					} catch(Storage::FileHolder::Error) {
//						set_error(EXOS::Error::FileDoesNotExist);
						set_error(EXOS::Error::InvalidEscapeSequence);
					}
				} else {
//					set_error(EXOS::Error::FileAlreadyExists);
					set_error(EXOS::Error::InvalidEscapeSequence);
				}
			}

			// TODO: what are appropriate error codes?.
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

			const auto next = channel->second.get();
			if(!channel->second.eof()) {
				set_error(EXOS::Error::NoError);
				set_c(next);
			} else {
				set_error(EXOS::Error::EndOfFileMetInRead);
			}
		break;
	}
}

void HostFSHandler::set_file_bundle(std::shared_ptr<Storage::FileBundle::FileBundle> bundle) {
	bundle_ = bundle;
}

std::vector<uint8_t> HostFSHandler::rom() {
	// Assembled and transcribed from hostfs.z80.
	return std::vector<uint8_t>{
		0x45, 0x58, 0x4f, 0x53, 0x5f, 0x52, 0x4f, 0x4d, 0x1b, 0x40, 0xc9, 0x00, 0x00, 0xfe, 0xff, 0x00,
		0x00, 0x00, 0x1d, 0x40, 0x00, 0x00, 0x04, 0x46, 0x49, 0x4c, 0x45, 0x0c, 0x00, 0x39, 0xc0, 0x3a,
		0xc0, 0x54, 0xc0, 0x5c, 0xc0, 0x61, 0xc0, 0x66, 0xc0, 0x6b, 0xc0, 0x70, 0xc0, 0x75, 0xc0, 0x7a,
		0xc0, 0x7f, 0xc0, 0x84, 0xc0, 0x89, 0xc0, 0x8e, 0xc0, 0xc9, 0x47, 0xed, 0xfe, 0xfe, 0x01, 0xa7,
		0xc0, 0xc5, 0x78, 0x01, 0x00, 0x00, 0x11, 0x01, 0x00, 0xf7, 0x1b, 0xc1, 0xa7, 0xc8, 0x78, 0xed,
		0xfe, 0xfe, 0x03, 0xc9, 0x47, 0xed, 0xfe, 0xfe, 0x02, 0xc3, 0x3f, 0xc0, 0xed, 0xfe, 0xfe, 0x03,
		0xc9, 0xed, 0xfe, 0xfe, 0x04, 0xc9, 0xed, 0xfe, 0xfe, 0x05, 0xc9, 0xed, 0xfe, 0xfe, 0x06, 0xc9,
		0xed, 0xfe, 0xfe, 0x07, 0xc9, 0xed, 0xfe, 0xfe, 0x08, 0xc9, 0xed, 0xfe, 0xfe, 0x09, 0xc9, 0xed,
		0xfe, 0xfe, 0x0a, 0xc9, 0xed, 0xfe, 0xfe, 0x0b, 0xc9, 0xed, 0xfe, 0xfe, 0x0c, 0xc9, 0xed, 0xfe,
		0xfe, 0x0d, 0xc9
	};
}
