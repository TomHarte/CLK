//
//  VHD.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 27/08/2025.
//  Copyright © 2025 Thomas Harte. All rights reserved.
//

#pragma once

#include "Storage/MassStorage/MassStorageDevice.hpp"
#include "Storage/FileHolder.hpp"

namespace Storage::MassStorage {

class VHD: public MassStorageDevice {
public:
	VHD(const std::string &file_name);

private:
	FileHolder file_;

	// MassStorageDevice.
	size_t get_block_size() override;
	size_t get_number_of_blocks() override;
	std::vector<uint8_t> get_block(size_t) override;
	void set_block(size_t, const std::vector<uint8_t> &) override;
};

}
