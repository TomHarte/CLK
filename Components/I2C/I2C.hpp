//
//  I2C.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 16/03/2024.
//  Copyright © 2024 Thomas Harte. All rights reserved.
//

#pragma once

#include <cstdint>
#include <unordered_map>

namespace I2C {

/// Provides the virtual interface for an I2C peripheral; attaching this to a bus
/// provides automatic protocol handling.
class Peripheral {

};

class Bus {
public:
	void set_data(bool pulled);
	bool data();

	void set_clock(bool pulled);
	bool clock();

	void set_clock_data(bool clock_pulled, bool data_pulled);

	void add_peripheral(Peripheral *, int address);

private:
	bool data_ = false;
	bool clock_ = false;
	std::unordered_map<int, Peripheral *> peripherals_;

	uint16_t input_ = 0xffff;
	int input_count_ = -1;

	Peripheral *active_peripheral_ = nullptr;
	uint16_t peripheral_response_ = 0xffff;
	int peripheral_bits_ = 0;

	enum class Phase {
		AwaitingStart,
		CollectingAddress,
		AwaitingByte,
		CollectingByte,
	} phase_ = Phase::AwaitingStart;
};

}