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
#include <vector>

namespace Enterprise {

struct HostFSHandler {
	struct MemoryAccessor {
		virtual uint8_t hostfs_read(uint16_t) = 0;
//		virtual void hostfs_write(uint16_t, uint8_t) = 0;

//		virtual uint8_t hostfs_user_read(uint16_t) = 0;
		virtual void hostfs_user_write(uint16_t, uint8_t) = 0;
	};

	HostFSHandler(MemoryAccessor &);

	/// Perform the internally-defined @c function given other provided state.
	/// These function calls mostly align with those in EXOSCodes.hpp
	void perform(uint8_t function, uint8_t &a, uint16_t &bc, uint16_t &de);

	/// Sets the bundle of files on which this handler should operate.
	void set_file_bundle(std::shared_ptr<Storage::FileBundle::FileBundle> bundle);

	/// @returns A suitable in-client filing system ROM.
	std::vector<uint8_t> rom();

private:
	MemoryAccessor &accessor_;
	std::shared_ptr<Storage::FileBundle::FileBundle> bundle_;

	using ChannelHandler = uint8_t;
	std::unordered_map<ChannelHandler, Storage::FileHolder> channels_;
};

};
