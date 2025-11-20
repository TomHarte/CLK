//
//  HostFSHandler.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 20/11/2025.
//  Copyright © 2025 Thomas Harte. All rights reserved.
//

#pragma once

#include "Storage/FileBundle/FileBundle.hpp"

#include <cstdint>
#include <memory>
#include <unordered_map>

namespace Enterprise {

struct HostFSHandler {
	HostFSHandler(uint8_t *ram);

	/// Perform the internally-defined @c function given other provided state.
	/// These function calls mostly align with those in EXOSCodes.hpp
	void perform(uint8_t function, uint8_t &a, uint16_t &bc, uint16_t &de);

	/// Sets the bundle of files on which this handler should operate.
	void set_file_bundle(std::shared_ptr<Storage::FileBundle::FileBundle> bundle);

private:
	uint8_t *ram_;
	std::shared_ptr<Storage::FileBundle::FileBundle> bundle_;

	using ChannelHandler = uint8_t;
	std::unordered_map<ChannelHandler, Storage::FileHolder> channels_;
};

};
