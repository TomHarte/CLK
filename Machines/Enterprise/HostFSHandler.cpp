//
//  HostFSHandler.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 20/11/2025.
//  Copyright © 2025 Thomas Harte. All rights reserved.
//

#include "HostFSHandler.hpp"
#include "EXOSCodes.hpp"

using namespace Enterprise;

HostFSHandler::HostFSHandler(uint8_t *ram) : ram_(ram) {}

void HostFSHandler::perform(uint8_t function, uint8_t &a, uint16_t &bc, uint16_t &de) {
	(void)function;
	(void)a;
	(void)bc;
	(void)de;
}
