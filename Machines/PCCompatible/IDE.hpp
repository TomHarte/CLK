//
//  IDE.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 26/08/2025.
//  Copyright © 2025 Thomas Harte. All rights reserved.
//

#pragma once

namespace PCCompatible {

struct IDE {
public:
	//
	// Drive interface.
	//
	void set_data(const uint16_t) {
	}
	uint16_t data() const {
		return 0xff;
	}

	void set_write_precompensation(const uint8_t) {}

	uint8_t error() const {
		return 0;
	}

	void set_sector_count(const uint8_t count) {
		sector_count_ = count;
	}
	uint8_t sector_count() const {
		return sector_count_;
	}

	void set_sector_number(const uint8_t number) {
		sector_number_ = number;
	}
	uint8_t sector_number() const {
		return sector_number_;
	}

	void set_cylinder_low(const uint8_t part) {
		cylinder_ = (cylinder_ & 0xff00) | part;
	}
	uint8_t cylinder_low() const {
		return uint8_t(cylinder_);
	}

	void set_cylinder_high(const uint8_t part) {
		cylinder_ = uint16_t((cylinder_ & 0x00ff) | (part << 8));
	}
	uint8_t cylinder_high() const {
		return uint8_t(cylinder_ >> 8);
	}

	void set_drive_head(const uint8_t drive_head) {
		drive_head_ = drive_head;
	}
	uint8_t drive_head() const {
		return drive_head_;
	}

	void set_command(const uint8_t) {
	}

	uint8_t status() {
		return 0xff;
	}

	//
	// Controller interface.
	//
	void set_controller_data(const uint8_t data) {
		controller_data_ = data;
	}
	uint8_t controller_data() const {
		return controller_data_;
	}

	void set_controller_status(const uint8_t status) {
		controller_status_ = status;
	}
	uint8_t controller_status() const {
		return controller_status_;
	}

private:
	uint8_t sector_count_;
	uint8_t sector_number_;
	uint16_t cylinder_;
	uint8_t drive_head_;

	uint8_t controller_data_;
	uint8_t controller_status_;
};

}
