//
//  CMOSRAM.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 20/03/2024.
//  Copyright © 2024 Thomas Harte. All rights reserved.
//

#pragma once

#include "../../../Components/I2C/I2C.hpp"

namespace Archimedes {

struct CMOSRAM: public I2C::Peripheral {
	void start(bool is_read) override {
		expecting_address_ = !is_read;
	}

	std::optional<uint8_t> read() override {
		return 0;
	}

	void write(uint8_t value) override {
		if(expecting_address_) {
			address_ = value;
		} else {
			// TODO: write to RAM.
		}
	}

private:
	bool expecting_address_ = false;
	uint8_t address_;
};

}
