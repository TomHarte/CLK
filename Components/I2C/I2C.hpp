//
//  I2C.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 16/03/2024.
//  Copyright © 2024 Thomas Harte. All rights reserved.
//

#pragma once

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
};

}
