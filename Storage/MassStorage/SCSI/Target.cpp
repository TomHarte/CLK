//
//  Target.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 17/08/2019.
//  Copyright © 2019 Thomas Harte. All rights reserved.
//

#include "Target.hpp"

using namespace SCSI::Target;

CommandState::CommandState(const std::vector<uint8_t> &data, const std::vector<uint8_t> &received) : data_(data), received_(received) {}

uint32_t CommandState::address() const {
	switch(data_.size()) {
		default:	return 0;
		case 6:
			return
				(uint32_t(data_[1]) << 16) |
				(uint32_t(data_[2]) << 8) |
				uint32_t(data_[3]);
		case 10:
		case 12:
			return
				(uint32_t(data_[1]) << 24) |
				(uint32_t(data_[2]) << 16) |
				(uint32_t(data_[3]) << 8) |
				uint32_t(data_[4]);
	}
}

uint16_t CommandState::number_of_blocks() const {
	switch(data_.size()) {
		default:	return 0;
		case 6:
			return uint16_t(data_[4]);
		case 10:
			return uint16_t((data_[7] << 8) | data_[8]);
	}
}

size_t CommandState::allocated_inquiry_bytes() const {
	// 0 means 256 bytes allocated for inquiry.
	return size_t(((data_[4] - 1) & 0xff) + 1);
}

CommandState::ModeSense CommandState::mode_sense_specs() const {
	ModeSense specs;

	specs.exclude_block_descriptors = (data_[1] & 0x08);
	specs.page_control_values = ModeSense::PageControlValues(data_[2] >> 5);
	specs.page_code = data_[2] & 0x3f;
	specs.subpage_code = data_[3];
	specs.allocated_bytes = number_of_blocks();

	return specs;
}

CommandState::ReadBuffer CommandState::read_buffer_specs() const {
	ReadBuffer specs;

	specs.mode = ReadBuffer::Mode(data_[1]&7);
	if(specs.mode > ReadBuffer::Mode::Reserved) specs.mode = ReadBuffer::Mode::Reserved;
	specs.buffer_id = data_[2];
	specs.buffer_offset = uint32_t((data_[3] << 16) | (data_[4] << 8) | data_[5]);
	specs.buffer_length = uint32_t((data_[6] << 16) | (data_[7] << 8) | data_[8]);

	return specs;
}

CommandState::ModeSelect CommandState::mode_select_specs() const {
	ModeSelect specs;

	specs.parameter_list_length = number_of_blocks();
	specs.content_is_vendor_specific = !(data_[1] & 0x10);
	specs.revert_to_default = (data_[1] & 0x02);
	specs.save_pages = (data_[1] & 0x01);

	return specs;
}
