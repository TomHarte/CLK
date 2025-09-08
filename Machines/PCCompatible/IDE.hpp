//
//  IDE.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 26/08/2025.
//  Copyright © 2025 Thomas Harte. All rights reserved.
//

#pragma once

#include "Outputs/Log.hpp"

namespace PCCompatible {

struct IDE {
public:
	//
	// Drive interface.
	//
	// TODO: probably all these belong directly on a drive; IDE = integrated drive electronics; the following
	// are all functions owned by the drive, not the controller.
	void set_data(const uint16_t data) {
		log_.info().append("Set data: %04x", data);
	}
	uint16_t data() const {
		log_.info().append("Read data");
		return 0xff;
	}

	void set_write_precompensation(const uint8_t precompensation) {
		log_.info().append("Set write precompensation: %02x", precompensation);
	}

	uint8_t error() const {
		log_.info().append("Read error");
		return 0;
	}

	void set_sector_count(const uint8_t count) {
		log_.info().append("Write sector count: %02x", count);
		sector_count_ = count;
	}
	uint8_t sector_count() const {
		log_.info().append("Read sector count: %02x", sector_count_);
		return sector_count_;
	}

	void set_sector_number(const uint8_t number) {
		log_.info().append("Write sector number: %02x", number);
		sector_number_ = number;
	}
	uint8_t sector_number() const {
		log_.info().append("Read sector number: %02x", sector_number_);
		return sector_number_;
	}

	void set_cylinder_low(const uint8_t part) {
		log_.info().append("Write cylinder low: %02x", part);
		cylinder_ = (cylinder_ & 0xff00) | part;
	}
	uint8_t cylinder_low() const {
		log_.info().append("Read cylinder low: %02x", uint8_t(cylinder_));
		return uint8_t(cylinder_);
	}

	void set_cylinder_high(const uint8_t part) {
		log_.info().append("Write cylinder high: %02x", part);
		cylinder_ = uint16_t((cylinder_ & 0x00ff) | (part << 8));
	}
	uint8_t cylinder_high() const {
		log_.info().append("Read cylinder high: %02x", uint8_t(cylinder_ >> 8));
		return uint8_t(cylinder_ >> 8);
	}

	void set_drive_head(const uint8_t drive_head) {
		log_.info().append("Write drive/head: %02x", drive_head);
		drive_head_ = drive_head;
	}
	uint8_t drive_head() const {
		log_.info().append("Read drive/head: %02x", drive_head_);
		return drive_head_;
	}

	void set_command(const uint8_t command) {
		log_.info().append("Command: %02x", command);
	}

	uint8_t status() {
		log_.info().append("Read status");
		return 0x40;	// i.e. drive ready.
	}

	//
	// Controller interface.
	//
	void set_controller_data(const uint8_t data) {
		log_.info().append("Write controller data: %02x", data);
		controller_data_ = data;
	}
	uint8_t controller_data() const {
		log_.info().append("Read controller data: %02x", controller_data_);
		return controller_data_;
	}

	void set_controller_status(const uint8_t status) {
		log_.info().append("Write controller status: %02x", status);
		controller_status_ = status;
	}
	uint8_t controller_status() const {
		log_.info().append("Read controller status: %02x", controller_status_);
		return controller_status_;
	}

private:
	uint8_t sector_count_;
	uint8_t sector_number_;
	uint16_t cylinder_;
	uint8_t drive_head_;

	uint8_t controller_data_;
	uint8_t controller_status_;

	mutable Log::Logger<Log::Source::IDE> log_;
};

}
