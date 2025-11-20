//
//  HostFSHandler.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 20/11/2025.
//  Copyright © 2025 Thomas Harte. All rights reserved.
//

#pragma once

#include <cstdint>

namespace Enterprise {

struct HostFSHandler {
	HostFSHandler(uint8_t *ram);
	void perform(uint8_t function, uint8_t &a, uint16_t &bc, uint16_t &de);

private:
	uint8_t *ram_;
};

};
