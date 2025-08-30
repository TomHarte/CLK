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

	uint16_t cylinders_;
	uint8_t heads_;
	uint8_t sides_;

	enum class Type {
		Fixed,
		Dynamic,
		Differencing,
	} type_;
	uint64_t data_offset_;

	// For dynamic VHDs.
	uint64_t table_offset_;
	uint32_t max_table_entries_;
	uint32_t block_size_;

	size_t total_blocks_;

	// MassStorageDevice.
	size_t get_block_size() const override;
	size_t get_number_of_blocks() const override;
	std::vector<uint8_t> get_block(size_t) const override;
	void set_block(size_t, const std::vector<uint8_t> &) override;
};

}
