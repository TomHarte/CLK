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
	const auto set_b = [&](const uint8_t ch) {
		bc = uint16_t((bc & 0xffff) | (ch << 8));
	};
	const auto set_c = [&](const uint8_t ch) {
		bc = (bc & 0xff00) | (ch);
	};
	const auto b = [&]() -> uint8_t {
		return bc >> 8;
	};
	const auto read_name = [&]() {
		// Get name.
		uint8_t length = accessor_.hostfs_read(de++);
		std::string name;
		while(length--) {
			name.push_back(char(accessor_.hostfs_read(de++)));
		}

		// Use the key file if no name is specified.
		if(name.empty()) {
			if(const auto key_file = bundle_->key_file(); key_file.has_value()) {
				name = *key_file;
			}
		}

		return name;
	};

	//
	// Functions that don't require an existing channel.
	//
	switch(function) {
		default: break;

		case uint8_t(EXOS::DeviceDescriptorFunction::Initialise):
			channels_.clear();
			set_error(EXOS::Error::NoError);
		return;

		case uint8_t(EXOS::DeviceDescriptorFunction::Interrupt):
		case uint8_t(EXOS::DeviceDescriptorFunction::BufferMoved):
			set_error(EXOS::Error::NoError);
		return;

		// Page 54.
		// Emprically: C contains the unit number.
		case uint8_t(EXOS::Function::OpenChannel): {
			if(a == 255) {
				set_error(EXOS::Error::ChannelIllegalOrDoesNotExist);
				break;
			}
			const auto name = read_name();

			try {
				channels_.emplace(a, bundle_->open(name, Storage::FileMode::ReadWrite));
				set_error(EXOS::Error::NoError);
			} catch(Storage::FileHolder::Error) {
				try {
					channels_.emplace(a, bundle_->open(name, Storage::FileMode::Read));
					set_error(EXOS::Error::NoError);
				} catch(Storage::FileHolder::Error) {
//					set_error(EXOS::Error::FileDoesNotExist);
					set_error(EXOS::Error::ProtectionViolation);
				}
			}
		}
		return;

		// Page 54.
		case uint8_t(EXOS::Function::CreateChannel): {
			if(a == 255) {
				set_error(EXOS::Error::ChannelIllegalOrDoesNotExist);
				break;
			}
			const auto name = read_name();

			try {
				channels_.emplace(a, bundle_->open(name, Storage::FileMode::Rewrite));
				set_error(EXOS::Error::NoError);
			} catch(Storage::FileHolder::Error) {
//				set_error(EXOS::Error::FileAlreadyExists);
				set_error(EXOS::Error::ProtectionViolation);
			}
		} return;

		case uint8_t(EXOS::Function::SpecialFunction):
			// Not supported;
			set_error(EXOS::Error::InvalidSpecialFunctionCode);
		return;
	}

	//
	// Functions from here require a channel already open.
	//
	const auto channel = channels_.find(a);
	if(channel == channels_.end()) {
		set_error(EXOS::Error::ChannelIllegalOrDoesNotExist);
		return;
	}
	auto &file = channel->second;

	switch(function) {
		default:
			printf("UNIMPLEMENTED function %d with A:%02x BC:%04x DE:%04x\n", function, a, bc, de);
		break;

		// Page 54.
		case uint8_t(EXOS::Function::CloseChannel):
			set_error(EXOS::Error::NoError);
			channels_.erase(channel);
		break;

		// Page 55.
		case uint8_t(EXOS::Function::ReadCharacter): {
			const auto next = file.get();
			if(file.eof()) {
				set_error(EXOS::Error::EndOfFileMetInRead);
			} else {
				set_b(next);
				set_error(EXOS::Error::NoError);
			}
		} break;

		// Page 55.
		case uint8_t(EXOS::Function::WriteCharacter): {
			if(file.put(b())) {
				set_error(EXOS::Error::NoError);
			} else {
				set_error(EXOS::Error::EndOfFileMetInRead);
			}
		} break;

		// Page 55.
		case uint8_t(EXOS::Function::ReadBlock): {
			set_error(EXOS::Error::NoError);
			while(bc) {
				const auto next = file.get();
				if(channel->second.eof()) {
					set_error(EXOS::Error::EndOfFileMetInRead);
					break;
				}

				accessor_.hostfs_user_write(de++, next);
				--bc;
			}
		} break;

		// Page 56.
		case uint8_t(EXOS::Function::WriteBlock): {
			set_error(EXOS::Error::NoError);
			while(bc) {
				const auto next = accessor_.hostfs_user_read(de);
				if(!file.put(next)) {
					set_error(EXOS::Error::EndOfFileMetInRead);
					break;
				}

				++de;
				--bc;
			}
		} break;

		// Page 56.
		case uint8_t(EXOS::Function::ReadChannelStatus):
			a = file.eof() ? 0xff : 0x00;
		break;

		// Page 56.
		case uint8_t(EXOS::Function::SetChannelStatus): {
			if(bc & 4) {
				// Protection byte is not supported.
				set_error(EXOS::Error::FunctionNotSupported);
				break;
			}

			if(bc & 1) {	// User is requesting a seek.
				auto pointer = de;
				uint32_t file_pointer;
				file_pointer = accessor_.hostfs_user_read(pointer++);
				file_pointer |= uint32_t(accessor_.hostfs_user_read(pointer++) << 8);
				file_pointer |= uint32_t(accessor_.hostfs_user_read(pointer++) << 16);
				file_pointer |= uint32_t(accessor_.hostfs_user_read(pointer++) << 24);

				if(!file.seek(file_pointer, Storage::Whence::SET)) {
					set_error(EXOS::Error::EndOfFileMetInRead);
					break;
				}
			}

			// Fill in both position and length.
			set_c(3);
			const uint32_t file_pointer = uint32_t(file.tell());
			const uint32_t file_length = uint32_t(file.stats().st_size);

			auto pointer = de;
			const auto write = [&](const uint32_t source) {
				accessor_.hostfs_user_write(pointer++, uint8_t(source >> 0));
				accessor_.hostfs_user_write(pointer++, uint8_t(source >> 8));
				accessor_.hostfs_user_write(pointer++, uint8_t(source >> 16));
				accessor_.hostfs_user_write(pointer++, uint8_t(source >> 24));
			};
			write(file_pointer);
			write(file_length);

			set_error(EXOS::Error::NoError);
		} break;
	}
}

void HostFSHandler::set_file_bundle(std::shared_ptr<Storage::FileBundle::FileBundle> bundle) {
	bundle_ = bundle;
}

std::vector<uint8_t> HostFSHandler::rom() {
	// Assembled and transcribed from hostfs.z80.
	return std::vector<uint8_t>{
		0x45, 0x58, 0x4f, 0x53, 0x5f, 0x52, 0x4f, 0x4d, 0x1b, 0x40, 0xc9, 0x00,
		0x00, 0xfe, 0xff, 0x00, 0x00, 0x00, 0x1d, 0x40, 0x00, 0x00, 0x04, 0x46,
		0x49, 0x4c, 0x45, 0x0c, 0x00, 0x39, 0xc0, 0x3a, 0xc0, 0x56, 0xc0, 0x5e,
		0xc0, 0x63, 0xc0, 0x68, 0xc0, 0x6d, 0xc0, 0x72, 0xc0, 0x77, 0xc0, 0x7c,
		0xc0, 0x81, 0xc0, 0x86, 0xc0, 0x8b, 0xc0, 0x97, 0xc0, 0xc9, 0x47, 0xed,
		0xfe, 0xfe, 0x01, 0xa7, 0xc0, 0xc5, 0x78, 0x01, 0x00, 0x00, 0x11, 0x01,
		0x00, 0xf7, 0x1b, 0xc1, 0xa7, 0xc8, 0x4f, 0x78, 0xed, 0xfe, 0xfe, 0x03,
		0x79, 0xc9, 0x47, 0xed, 0xfe, 0xfe, 0x02, 0xc3, 0x3f, 0xc0, 0xed, 0xfe,
		0xfe, 0x03, 0xc9, 0xed, 0xfe, 0xfe, 0x04, 0xc9, 0xed, 0xfe, 0xfe, 0x05,
		0xc9, 0xed, 0xfe, 0xfe, 0x06, 0xc9, 0xed, 0xfe, 0xfe, 0x07, 0xc9, 0xed,
		0xfe, 0xfe, 0x08, 0xc9, 0xed, 0xfe, 0xfe, 0x09, 0xc9, 0xed, 0xfe, 0xfe,
		0x0a, 0xc9, 0xed, 0xfe, 0xfe, 0x0b, 0xc9, 0xed, 0xfe, 0xfe, 0x0c, 0x11,
		0x16, 0xc0, 0x0e, 0x01, 0xf7, 0x13, 0xc9, 0xed, 0xfe, 0xfe, 0x0d, 0xc9
	};
}
